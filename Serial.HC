
#define PORT0 0x3f8

I64 UartInit() {
  // Values from OSDev Serial Port Wiki
  OutU8(PORT0 + 1, 0x00);
  OutU8(PORT0 + 3, 0x80);
  OutU8(PORT0 + 0, 0x01); // 115200 baud
  OutU8(PORT0 + 1, 0x00);
  OutU8(PORT0 + 3, 0x03);
  OutU8(PORT0 + 2, 0xC7);
  OutU8(PORT0 + 4, 0x0B);
  OutU8(PORT0 + 4, 0x1E);
  OutU8(PORT0 + 0, 0xAE);

  if(InU8(PORT0+0) != 0xAE) {
    "Uart Init failed!\n";
    return -1;
  }

  OutU8(PORT0+4, 0x0F);
  "Uart init success!\n";
  return 0;
}

U8 UartReady() {
  return InU8(PORT0+5)&0x1;
}

U8 UartRxChar() {
  while(UartReady() == 0) Yield();

  return InU8(PORT0);
}

// Bit 8 is set if char was received
I64 UartRxCharNonBlk() {
  if (UartReady()) {
    return InU8(PORT0)(I64) | (1 << 8);
  }
}

U8* UartRxStr() {
  U8 buf[256];
  I64 i = 0;

  while(i<255) {
    buf[i]=UartRxChar();
    if(buf[i]=='\n' || buf[i]=='\0') {
      break;
    }
    i++;
  }
  buf[i]=0;
  return MStrPrint("%s\n", buf);
}

U8 UartTxEmpty() {
  return (InU8(PORT0+5)&0x20) != 0;
}

U0 UartTxChar(U8 c) {
  while(UartTxEmpty() == 0) Yield;

  OutU8(PORT0, c);
}

U0 UartTxStr(U8 *s) {
  while(*s) {
    UartTxChar(*s);
    s++;
  }
}

UartInit();

// U8 *str = UartReadStr();
// "Uart Output: %s", str;
// Free(str);

/*
U8 *str;
UartTxStr("\n\n#####################\n");
UartTxStr("Hello from temple!\n");
UartTxStr("\n#####################\n\n");

I64 i;
for(i=0;i<50;++i){
  if(i%15==0) {
    UartTxStr("FizzBuzz\n");
  } else if(i%3==0) {
    UartTxStr("Fizz\n");
  } else if(i%5==0) {
    UartTxStr("Buzz\n");
  } else {
    str=MStrPrint("%d\n", i);
    UartTxStr(str);
    Free(str);
  }
}
*/

