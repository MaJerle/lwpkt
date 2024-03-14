import enum, struct, queue

'''
This file is currently in the development 
and shall not be used by the final application
'''

class LwPKT(object):
    class LwPKT_Receive(object):

        class State(enum.Enum):
            START  = 0
            FROM   = 1
            TO     = 2
            CMD    = 3     
            FLAGS  = 4     
            LEN    = 5    
            DATA   = 6  
            CRC    = 7
            STOP   = 8 
            END    = 9

        def __init__(self):
            self.data_raw = bytearray()
            self.state = LwPKT.LwPKT_Receive.State.START
            self.len = 0
            self.data = bytearray()
            self.flags = 0
            self.cmd = 0
            self.crc = 0
            self.index = 0

    '''
    LwPKT object
    '''
    def __init__(self):
        self.opt_addr = True
        self.opt_addr_ext = True
        self.opt_cmd = True
        self.opt_crc = True
        self.opt_crc32 = True
        self.opt_flags = True
        self.our_addr = 0
        self.rx_data = queue.Queue()
        self.rx = LwPKT.LwPKT_Receive()

    # Generate the packet that can be sent out
    def generate_packet(self, data:bytes, cmd:int = 0, addr_to:int = 0, flags:int = 0) -> bytes:
        data_out = bytearray()
        
        # Start byte goes here
        data_out.append(0xAA)
        
        # Add address
        if self.opt_addr:
            if self.opt_addr_ext:
                data_out += self.varint_encode(self.our_addr)
                data_out += self.varint_encode(addr_to)
            else:
                data_out.append(self.our_addr & 0xFF)
                data_out.append(addr_to & 0xFF)

        # Add custom flags
        if self.opt_flags:
            data_out += self.varint_encode(flags)

        # Add command
        if self.opt_cmd:
            data_out.append(cmd & 0xFF)

        # Add data length, then actual data
        data_out += self.varint_encode(len(data))
        if len(data) > 0:
            data_out += data

        # Calc CRC of all data (except start byte)
        if self.opt_crc:
            crc = 0xFFFFFFFF if self.opt_crc32 else 0
            for val in data_out[1:]:
                crc = self.crc_in(crc, val)
            if self.opt_crc32:
                crc = crc ^ 0xFFFFFFFF
                data_out += struct.pack('<I', crc)
            else:
                data_out.append(crc)

        # Packet ends here
        data_out.append(0x55)
        return data_out
    
    # Encode variable integer to bytes, little endian first
    def varint_encode(self, num:int) -> bytes:
        out = bytearray()
        while True:
            val = num & 0x7F
            if num > 0x7F:
                val = val | (0x80)
            num = num >> 7
            out.append(val)
            if num == 0:
                break
        return out
    
    # Add crc entry and calculate output
    # Aim is to get single byte entry
    def crc_in(self, crc_old_val:int, new_byt:int) -> int:
        for i in range(8):
            m = (crc_old_val ^ new_byt) & 0x01
            crc_old_val = crc_old_val >> 1
            if m:
                crc_old_val = crc_old_val ^ (0xEDB88320 if self.opt_crc32 else 0x8C)
            new_byt = new_byt >> 1
        return crc_old_val
    
    # Write RX data to packet 
    def write_rx_data(self, data:bytearray):
        for d in data: self.rx_data.put_nowait(d)

    # Process RX data
    # TODO: Implement
    def process_rx_data(self):
        ret = False
        while not self.rx_data.empty():
            ch = self.rx_data.get_nowait()

            if self.rx.state == LwPKT.LwPKT_Receive.State.START:
                if ch == 0xAA:
                    self.rx.state = LwPKT.LwPKT_Receive.State.FROM
            elif self.rx.state == LwPKT.LwPKT_Receive.State.FROM:
                pass
        return ret

if __name__ == '__main__':
    print('main')
    pkt = LwPKT()

    data = bytearray("Hello World\r\n".encode('utf-8'))
    print('data', len(data))
    pkt.our_addr = 0x12345678
    pkt.opt_addr = True
    pkt.opt_addr_ext = True
    pkt.opt_cmd = True
    pkt.opt_flags = True
    pkt.opt_crc = True

    # Encode
    packet = pkt.generate_packet(data, addr_to = 0x87654321, flags = 0xACCE550F, cmd=0x85)
    print('packet', len(packet))
    print('packet_data', ', '.join(['0x{:02X}'.format(i) for i in packet]))

    # Decode
    pkt.write_rx_data(packet)
    decoded = pkt.process_rx_data()
    print('decoded', decoded)
