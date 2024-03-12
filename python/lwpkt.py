import enum, struct

class LwPKT(object):

    '''
    LwPKT object
    '''
    def __init__(self):
        self.opt_addr = True
        self.opt_addr_ext = True
        self.opt_cmd = True
        self.opt_crc = True
        self.opt_flags = True
        self.our_addr = 0x12

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

        # Calculate CRC
        if self.opt_crc:
            pass

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

if __name__ == '__main__':
    print('main')
    pkt = LwPKT()

    data = bytearray()
    data += struct.pack('<H', 0x3344)
    print('data', len(data))
    packet = pkt.generate_packet(data, cmd=0x12, addr_to = 0x10, flags = 0x15)
    print('packet', len(packet))
    print('packet_data', ', '.join(['0x{:02X}'.format(i) for i in packet]))