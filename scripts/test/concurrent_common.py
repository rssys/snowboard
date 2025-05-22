

class TestCase:
    def __init__(self, write_addr, read_addr, write_ins, read_ins, write_value, 
                 read_value, write_byte, read_byte, double_read, write_id, read_id):
        self.write_addr     = write_addr
        self.read_addr      = read_addr
        self.write_ins      = write_ins
        self.read_ins       = read_ins
        self.write_value    = write_value
        self.read_value     = read_value
        self.write_byte     = write_byte
        self.read_byte      = read_byte
        self.double_read    = double_read
        self.write_id       = write_id
        self.read_id        = read_id
    
    def to_list(self):
        return [self.write_addr, self.read_addr, self.write_ins, self.read_ins,
                self.write_value, self.read_value, self.write_byte, self.read_byte,
                self.double_read, self.write_id, self.read_id]
    
    def format_for_log(self, extra_info=""):
        return (f"{hex(self.write_addr)} {hex(self.read_addr)} {hex(self.write_ins)} "
                f"{hex(self.read_ins)} {hex(self.write_value)} {hex(self.read_value)} "
                f"{self.write_byte} {self.read_byte} {self.double_read} "
                f"{self.write_id} {self.read_id}{' ' + extra_info if extra_info else ''}")
