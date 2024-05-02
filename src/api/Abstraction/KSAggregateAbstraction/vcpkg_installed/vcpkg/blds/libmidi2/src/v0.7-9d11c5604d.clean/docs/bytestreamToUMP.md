# bytestreamToUMP
Class used for translating between a raw MIDI 1.0 Byte stream and UMP

## Public variables

#### uint8_t defaultGroup = 0;

This allows the application to set the group used when creating UMP messages. This is zero based.

#### bool outputMIDI2 = false;

By default ```bytestreamToUMP``` will output MIDI 1.0 Channel voice message (Message Type 2). Set this to true to output 
MIDI 2 Channel Voice Messages (Message Type 4).

This uses the Translation methods described in **Universal MIDI Packet (UMP) Format
and MIDI 2.0 Protocol** Version 1.1.

This class attempts to convert RPN and NRPN Control Change messages into UMP RPN and NRPN messages. This may cause some 
issues with Some devices that do not send LSB Data vale (CC #38).

## Common methods
### void bytestreamParse(uint8_t midi1Byte)
Process incoming MIDI 1.0 Byte stream

### bool availableUMP()
Check if there are available UMP packets after processing the Byte Stream

### uint32_t readUMP()
Return a 32Bit word for a UMP Packet. A bytestream conversion may create several UMP packets.



## Example: Using bytestreamToUMP 

```c++

bytestreamToUMP DIN2UMP;

int main(){
    DIN2UMP.defaultGroup = 2; //Set Group 3
    
    while(1){
        //Read from DIN Port
        //-------------------
        if (uart_is_readable(uart1)) {
            uint8_t ch = uart_getc(uart1);
            if(ch == 0xFE) continue; //Skip ActiveSense
            DIN2UMP.midi1BytestreamParse(ch);
            while(DIN2UMP.availableUMP()){
                uint32_t ump = DIN2UMP.readUMP();
                //Process UMP 
            }
        }
    }
}
```

