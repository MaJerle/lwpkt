import enum, struct, queue

'''
This file is currently in the development 
and shall not be used by the final application
'''

class LwPKT(object):
    class LwPKT_Packet(object):

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
            self.state = LwPKT.LwPKT_Packet.State.START
            self.len = 0
            self.data = bytearray()
            self.pkt_from = 0
            self.pkt_to = 0
            self.flags = 0
            self.cmd = 0
            self.crc = 0
            self.crc_recv = 0
            self.index = 0

        def go_to_state(self, state:State):
            self.index = 0
            self.state = state

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

        # Contains raw RX bytes
        self.rx_data = queue.Queue()

        # Contains receive object
        # Each new packet is restarted (and reconstructed)
        self.rx = LwPKT.LwPKT_Packet()

        # Contains queue of valid receive packets
        self.rx_packets = queue.Queue()

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

    #
    # Process RX data
    #
    def rx_process(self):
        '''
        Process the RX data.

        Data must be previously written using write_rx_data function.
        Function will return True when valid packet is ready to be read, or false if there has been no new packets added to the queue.
        User can then use get_rx_data() function to retrieve packet
        '''
        ret = False
        while not self.rx_data.empty():
            ch = self.rx_data.get_nowait()

            match self.rx.state:
                case LwPKT.LwPKT_Packet.State.START:
                    if ch == 0xAA:
                        self.rx = LwPKT.LwPKT_Packet()
                        self.rx.crc = 0xFFFFFFFF if self.opt_crc32 else 0
                        self.rx_go_to_next_state()

                case LwPKT.LwPKT_Packet.State.FROM:
                    self.rx.crc = self.crc_in(self.rx.crc, ch)
                    if self.opt_addr_ext:
                        self.rx.pkt_from |= (ch & 0x7F) << (7 * self.rx.index)
                        self.rx.index += 1
                    else:
                        self.rx.pkt_from = ch

                    if not self.opt_addr_ext or (ch & 0x80) == 0:
                        self.rx_go_to_next_state()

                case LwPKT.LwPKT_Packet.State.TO:
                    self.rx.crc = self.crc_in(self.rx.crc, ch)
                    if self.opt_addr_ext:
                        self.rx.pkt_to |= (ch & 0x7F) << (7 * self.rx.index)
                        self.rx.index += 1
                    else:
                        self.rx.pkt_to = ch

                    if not self.opt_addr_ext or (ch & 0x80) == 0:
                        self.rx_go_to_next_state()

                case LwPKT.LwPKT_Packet.State.FLAGS:
                    self.rx.crc = self.crc_in(self.rx.crc, ch)
                    self.rx.flags |= (ch & 0x7F) << (7 * self.rx.index)
                    self.rx.index += 1
                    if (ch & 0x80) == 0:
                        self.rx_go_to_next_state()

                case LwPKT.LwPKT_Packet.State.CMD:
                    self.rx.crc = self.crc_in(self.rx.crc, ch)
                    self.rx.cmd = ch
                    self.rx_go_to_next_state()

                case LwPKT.LwPKT_Packet.State.LEN:
                    self.rx.crc = self.crc_in(self.rx.crc, ch)
                    self.rx.len |= (ch & 0x7F) << (7 * self.rx.index)
                    self.rx.index += 1
                    if (ch & 0x80) == 0:
                        self.rx_go_to_next_state()

                case LwPKT.LwPKT_Packet.State.DATA:
                    self.rx.crc = self.crc_in(self.rx.crc, ch)
                    self.rx.data.append(ch)
                    if len(self.rx.data) == self.rx.len:
                        self.rx_go_to_next_state()

                case LwPKT.LwPKT_Packet.State.CRC:
                    crc_len = 4 if self.opt_crc32 else 1
                    if self.rx.index < crc_len:
                        self.rx.crc_recv |= ch << (8 * self.rx.index)
                        self.rx.index += 1

                    if self.rx.index == crc_len:
                        if self.opt_crc32:
                            self.rx.crc ^= 0xFFFFFFFF
                        if self.rx.crc == self.rx.crc_recv:
                            self.rx_go_to_next_state()
                        else:
                            self.rx_go_to_next_state()

                case LwPKT.LwPKT_Packet.State.STOP:
                    self.rx_packets.put_nowait(self.rx)
                    self.rx_go_to_next_state()
                    
                    # At least one packet has been added to queue
                    ret = True

                # Handle default situation
                case _:
                    self.rx.go_to_state(LwPKT.LwPKT_Packet.State.START)
        return ret
    
    # Get the packet
    def rx_get_packet(self) -> LwPKT_Packet|bool:
        if not self.rx_packets.empty():
            return self.rx_packets.get_nowait()
        return False

    # Put RX to next state, according to the configuration options
    def rx_go_to_next_state(self):     
        new_state = LwPKT.LwPKT_Packet.State.END
        match self.rx.state:
            case LwPKT.LwPKT_Packet.State.START:
                if self.opt_addr: new_state = LwPKT.LwPKT_Packet.State.FROM
                elif self.opt_flags: new_state = LwPKT.LwPKT_Packet.State.FLAGS
                elif self.opt_cmd: new_state = LwPKT.LwPKT_Packet.State.CMD
                else: new_state = LwPKT.LwPKT_Packet.State.LEN
            case LwPKT.LwPKT_Packet.State.TO:
                if self.opt_flags: new_state = LwPKT.LwPKT_Packet.State.FLAGS
                elif self.opt_cmd: new_state = LwPKT.LwPKT_Packet.State.CMD
                else: new_state = LwPKT.LwPKT_Packet.State.LEN
            case LwPKT.LwPKT_Packet.State.FLAGS:
                if self.opt_cmd: new_state = LwPKT.LwPKT_Packet.State.CMD
                else: new_state = LwPKT.LwPKT_Packet.State.LEN
            case LwPKT.LwPKT_Packet.State.CMD:
                new_state = LwPKT.LwPKT_Packet.State.LEN
            case LwPKT.LwPKT_Packet.State.LEN:
                if self.rx.len > 0: new_state = LwPKT.LwPKT_Packet.State.DATA
                elif self.opt_crc: new_state = LwPKT.LwPKT_Packet.State.CRC
                else: new_state = LwPKT.LwPKT_Packet.State.STOP
            case LwPKT.LwPKT_Packet.State.DATA:
                if self.opt_crc: new_state = LwPKT.LwPKT_Packet.State.CRC
                else: new_state = LwPKT.LwPKT_Packet.State.STOP
            case LwPKT.LwPKT_Packet.State.CRC:
                new_state = LwPKT.LwPKT_Packet.State.STOP
            case LwPKT.LwPKT_Packet.State.FROM:
                # There is no other way to go than into TO state
                new_state = LwPKT.LwPKT_Packet.State.TO
            case LwPKT.LwPKT_Packet.State.STOP:
                new_state = LwPKT.LwPKT_Packet.State.START
            case _:
                new_state = LwPKT.LwPKT_Packet.State.START
        if new_state != LwPKT.LwPKT_Packet.State.END:
            self.rx.go_to_state(new_state)

if __name__ == '__main__':
    print('main')
    pkt = LwPKT()

    for i in range(1 << 6):
        data = bytearray("Hello World\r\n".encode('utf-8'))

        # Variables
        addr_to = 0x87654321
        addr_our = 0x12345678
        flags = 0xACCE550F
        cmd = 0x85

        # Setup config for data
        pkt.opt_addr = True if i & 0x01 else False
        pkt.opt_addr_ext = True if i & 0x02 else False
        pkt.opt_flags = True if i & 0x04 else False
        pkt.opt_cmd = True if i & 0x08 else False
        pkt.opt_crc = True if i & 0x10 else False
        pkt.opt_crc32 = True if i & 0x20 else False

        if not pkt.opt_addr_ext:
            addr_to &= 0xFF
            addr_our &= 0xFF

        # Setup the values now
        pkt.our_addr = addr_our
        
        print('Test case ' + str(i + 1))

        # Encode
        packet = pkt.generate_packet(data, addr_to = addr_to, flags = flags, cmd=cmd)
        print('packet', len(packet))
        print('packet_data', ', '.join(['0x{:02X}'.format(i) for i in packet]))

        # Write data back and act as a processing function
        pkt.write_rx_data(packet)

        # Process the packet.
        # If it returns true, then at least one packet has been added to queue
        if pkt.rx_process():
            while True:
                # Get the packet
                decoded_packet = pkt.rx_get_packet()
                if not decoded_packet: break
                
                test_ok = False
                if pkt.opt_addr and pkt.our_addr != decoded_packet.pkt_from:
                    print('receive address mismatch!')
                elif pkt.opt_addr and decoded_packet.pkt_to != addr_to:
                    print('destination address mismatch!')
                elif pkt.opt_flags and decoded_packet.flags != flags:
                    print('flags mismatch!')
                elif decoded_packet.len != len(data):
                    print('data len mismatch!')
                else:
                    test_ok = True

                    # Now compare content
                    for idx, byt in enumerate(decoded_packet.data):
                        if byt != data[idx]:
                            test_ok = False
                            print('data mismatch')
                            break
                
                if test_ok:
                    print('test OK')
                else:
                    print('test fail')
                    exit()
        else:
            print('Test fail')
        print('---')