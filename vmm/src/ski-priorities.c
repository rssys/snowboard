/*
 * SKI - Systematic Kernel Interleaving explorer (http://ski.mpi-sws.org)
 *
 * Copyright (c) 2013-2015 Pedro Fonseca
 *
 *
 * This work is licensed under the terms of the GNU GPL, version 3.  See
 * the GPL3 file in SKI's top-level directory.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "config.h"
#include "cpu.h"
#include "disas.h"
#include "tcg.h"
#include "qemu-barrier.h"
#include "ski.h"
#include "ski-priorities.h"

/* FORMAT:
   - the cpus and the ints fields must have the {} characters
   - the n field must not have the {} characters
   - the int value -1 serves to indicate the no-int case
   - the presence of multiple groups, which must be separated by a |, means that their order should be randomized
   - if just a single group is present a | character at the end indicates that it should be randomized

  	  cpu{0,1,2,3};int{-1,1,4,5,7};3
 	  cpu{0,1,2,3};int{-1,1,4,5,7};1|
	  cpu{0,1,2,3};int{-1,1,4,5,7};1|cpu{0};int{1,3};3
	  cpu{0};int{-1,1,4,5,7};1
*/

static ski_config_priorities_entry plist[SKI_MAX_CONFIG_PRIORITIES_PER_LINE];
static int plist_n;


/* Random implementation from the POSIX */
static unsigned long ski_config_priorities_random_next = 1;

/* RAND_MAX assumed to be 32767 */
int ski_config_priorities_random(void) {
   ski_config_priorities_random_next = ski_config_priorities_random_next * 1103515245 + 12345;
   return((unsigned)(ski_config_priorities_random_next/65536) % 32768);
}

void ski_config_priorities_random_init(unsigned  seed) {
   SKI_INFO_NOCPU("ski_config_priorities_random_init: seed = %u\n", seed);
   ski_config_priorities_random_next = seed;
}



void ski_config_priorities_parseline(char *arg_str)
{
	char concurrent_sep='|';
	char list_sep = ',';
	int i, j;
	char str[SKI_MAX_CONFIG_PRIORITIES_LENGHT];
	int res;
	char *cur_group = str;
	char *next_group = 0;

	int should_randomize = 0;

	regex_t regex;
	int reti;
	char msgbuf[512];

	// Operate on a copy of the string
	strncpy(str,arg_str,SKI_MAX_CONFIG_PRIORITIES_LENGHT-1);
	str[SKI_MAX_CONFIG_PRIORITIES_LENGHT-1]=0;

	int plist_n_old = plist_n;

	SKI_INFO_NOCPU("ski_config_priorities_parseline: %s\n", str);

	/* Compile regular expression */
	reti = regcomp(&regex, "cpu{\\([^}]*\\)};int{\\([^}]*\\)};\\([:alnum]*\\)", 0);
	if (reti){ 
		fprintf(stderr, "Could not compile regex\n"); 
		exit(1); 
	}

	while (1){
		int max_matches = 4;
		regmatch_t matches[max_matches];
		int cpus_matches[SKI_MAX_CONFIG_PRIORITIES_FIELD_ENTRIES];
		int cpus_matches_n = 0;
		int ints_matches[SKI_MAX_CONFIG_PRIORITIES_FIELD_ENTRIES];
		int ints_matches_n = 0;
		int n_match;
		if(strlen(cur_group)==0){
			break;
		}
		/* Execute regular expression */
		reti = regexec(&regex,cur_group, max_matches, matches, 0);
		if (!reti){
			//printf("Match\n");
			//printf("   match[0] entire grp  %d-%d\n", matches[0].rm_so, matches[0].rm_eo);
			//printf("   match[1] (cpus_list) %d-%d\n", matches[1].rm_so, matches[1].rm_eo);
			//printf("   match[2] (ints_list) %d-%d\n", matches[2].rm_so, matches[2].rm_eo);
			//printf("   match[3] (n)         %d-%d\n", matches[3].rm_so, matches[3].rm_eo);
			int n;
			char *cur_entry;

			cur_entry = cur_group + matches[1].rm_so; 
			while ((cur_entry >= cur_group) && (cur_entry < (cur_group + matches[1].rm_eo))){
				assert(cur_entry >= cur_group && cur_entry < cur_group + strlen(cur_group));
				//printf(" cur_entry = %s\n", cur_entry);
				int res = sscanf(cur_entry,"%d", &n);
				//printf("    read value (cpus): %d\n", n);
				cpus_matches[cpus_matches_n] = n;
				cpus_matches_n++;
				assert(res);
				cur_entry = strchr(cur_entry,(int)list_sep) + 1; 
			}

			cur_entry = cur_group + matches[2].rm_so; 
			while ((cur_entry >= cur_group) && (cur_entry < (cur_group + matches[2].rm_eo))){
				assert(cur_entry >= cur_group && cur_entry < cur_group + strlen(cur_group));
				//printf(" cur_entry = %s\n", cur_entry);
				int res = sscanf(cur_entry,"%d", &n);
				//printf("    read value (ints): %d\n", n);
				ints_matches[ints_matches_n] = n;
				ints_matches_n++;
				assert(res);
				cur_entry = strchr(cur_entry,(int)list_sep) + 1; 
			}

			int res = sscanf(cur_group + matches[3].rm_so,"%d", &n);
			//printf("    read value (n): %d\n", n);
			n_match = n;
			
			for (i=0;i<cpus_matches_n;i++){
				for(j=0;j<ints_matches_n;j++){
					ski_config_priorities_entry *p = &plist[plist_n]; 
					p->cpu = cpus_matches[i];
					p->i = ints_matches[j];
					p->n = n_match;
					plist_n++;
				}
			}
		}
		else if (reti == REG_NOMATCH){
			printf("No match\n");
			assert(0);
		}
		else{
			regerror(reti, &regex, msgbuf, sizeof(msgbuf));
			fprintf(stderr, "Regex match failed: %s\n", msgbuf);
			exit(1);
		}
		if (!(next_group = strchr(cur_group,(int) concurrent_sep))){
			break;
		}
		should_randomize = 1;
		cur_group = next_group + 1;	
	}

	for(i=plist_n_old;i<plist_n;i++){
		ski_config_priorities_entry *p = &plist[i];
		SKI_TRACE_NOCPU(" i: %02d cpu: %d int: %d n: %d\n", i, p->cpu, p->i, p->n);
	}

	if(should_randomize){
		/* The modern version of the Fisher-Yates shuffle algorithm:
		   To shuffle an array a of n elements (indices 0..n-1):
				  for i from n - 1 downto 1 do
					   j <- random integer with 0 <= j <= i
					   exchange a[j] and a[i]
		*/
		ski_config_priorities_entry tmp;
		int start = plist_n_old;
		int end = plist_n - 1;
		for(i=end; i>=start; i--){
			j = (ski_config_priorities_random() % (i - start + 1)) + start;
			memcpy(&tmp, plist + i, sizeof(ski_config_priorities_entry));
			memcpy(plist + i, plist + j, sizeof(ski_config_priorities_entry));
			memcpy(plist + j, &tmp, sizeof(ski_config_priorities_entry));
		}
		SKI_TRACE_NOCPU("After randomizing:\n");
		for(i=plist_n_old;i<plist_n;i++){
			ski_config_priorities_entry *p = &plist[i];
			SKI_TRACE_NOCPU(" i: %02d cpu: %d int: %d n: %d\n", i, p->cpu, p->i, p->n);
		}

	}

	/* Free compiled regular expression if you want to use the regex_t again */
	regfree(&regex);
}


void ski_config_priorities_parse_file(CPUState *env, char *filename, int seed)
{
	int i;
	FILE *fp = fopen(filename,"r");
	int seed_high_granularity = (int) (seed / 10);
	assert(fp);

	SKI_TRACE("ski_config_priorities_parse_file: opend file %s to read configuration\n", filename);
	SKI_TRACE("ski_config_priorities_parse_file: seeding the pseudo-random generator with: %d\n", seed);
	SKI_TRACE("ski_config_priorities_parse_file: reducing seed to: %d\n", seed_high_granularity);

	ski_config_priorities_random_init(seed_high_granularity);

	plist_n = 0;

	while(1){
		int res;
		char buffer[512];

		res = fgets(buffer, 512-1, fp);
		if(!res){
			break;
		}
		if(buffer[strlen(buffer)-1]=='\n')
			buffer[strlen(buffer)-1] = 0;
		ski_config_priorities_parseline(buffer);	
	}
	fclose(fp);

	ski_threads_reset(env);
	for(i=0;i<plist_n;i++){
		ski_config_priorities_entry *p = &plist[i];
		ski_threads_insert(env, p->cpu, p->i, p->n);
		//printf("Adding the following priorities: i: %02d cpu: %d int: %d n: %d\n", i, p->cpu, p->i, p->n);
	}

	SKI_TRACE("ski_config_priorities_parse_file: finished parsing the intial priorities file\n");
	ski_tc_dump_all(env);

}

/*void main(){
	int i;
	
	ski_config_priorities_parse_file("init.priorities", time(0));
}
*/

