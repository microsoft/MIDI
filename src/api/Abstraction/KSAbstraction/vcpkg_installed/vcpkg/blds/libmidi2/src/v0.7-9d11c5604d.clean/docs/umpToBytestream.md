# umpToBytestream
Class used for translating from UMP to a MIDI 1.0 Bytestream

## Public variables

#### uint8_t group = 0;

Group is set to the current group of the translated UMP


## Common methods
### void UMPStreamParse(uint32_t UMP)
Process incoming UMP's

### bool availableBS()
Check if there are available Byte Stream bytes available.

### uint8_t readBS()
Return the next byte.

## Example: Using umpToBytestream 

```c++

umpToBytestream UMPConvertForDIN;

int main(){
   ...
    while(1){
        if (isUMPAvailable()) {
            uint32_t ump = get_UMP();
            UMPConvertForDIN.UMPStreamParse(ump);
            while (UMPConvertForDIN.availableBS()) {
                uint8_t byte = UMPConvertForDIN.readBS();
                if (UMPConvertForDIN.group == 0) { //Check if data is on correct group
                    //Write byte to DIN Port
                }
            }
        }
    }
}
```

