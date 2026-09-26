
#define XM_SOH        0x01
#define XM_EOT        0x04
#define XM_ACK        0x06
#define XM_NAK        0x15
#define XM_CAN        0x18
#define XM_BLOCK_SIZE 128
#define XM_MAX_RETRIES 10

I64 XmodemSendBlock(U8 blockNum, U8 *data, I64 len) {
  U8 buf[XM_BLOCK_SIZE];
  U8 checksum;
  U8 c;
  I64 i, retries;

  MemSet(buf, 0x1A, XM_BLOCK_SIZE);
  MemCpy(buf, data, len);

  checksum = 0;
  for (i = 0; i < XM_BLOCK_SIZE; i++)
    checksum += buf[i];

  for (retries = 0; retries < XM_MAX_RETRIES; retries++) {
    UartTxChar(XM_SOH);
    UartTxChar(blockNum);
    UartTxChar(255 - blockNum);
    for (i = 0; i < XM_BLOCK_SIZE; i++)
      UartTxChar(buf[i]);
    UartTxChar(checksum);

    c = UartRxChar();
    if (c == XM_ACK) return 0;
    if (c == XM_CAN) return -1;
  }
  return -1;
}

U8 *XmodemLoadFile(U8 *filename, I64 *sizeOut) {
  U8 *data = FileRead(filename, sizeOut);
  if (!data) {
    "Xmodem: could not read file %s\n", filename;
    return NULL;
  }
  "file '%s' successfully read with size 0x%08x\n", filename, *sizeOut;
  return data;
}

I64 XmodemSendFile(U8 *filename) {
  I64 size = 0;
  U8 *data = XmodemLoadFile(filename, &size);
  if (!data) return -1;

  I64 off = 0, chunk;
  U8 blockNum = 1;
  U8 c;
  I64 retries, ok = TRUE;

  "File found with size 0x%08x\n", size;
  // clear Rx buffer
  while((UartRxCharNonBlk()&0x100)) Yield();
  // signal bootloader upload request
  "Sending start char 'u'\n";
  UartTxChar('u');

  "Waiting for NAK\n";
  // Wait for receiver's initial handshake byte (checksum-mode NAK)
  do {
    c = UartRxChar();
  } while ((c != XM_NAK) && (c != XM_CAN));

  if (c == XM_CAN) {
    "Xmodem: receiver cancelled before start\n";
    Free(data);
    return -1;
  }

  if (c == XM_NAK)
    "NAK received. continuing...\n";

  progress1_max=size;
  while (off < size) {
    chunk = size - off;
    if (chunk > XM_BLOCK_SIZE) chunk = XM_BLOCK_SIZE;

    if (XmodemSendBlock(blockNum, data + off, chunk) != 0) {
      "Xmodem: transfer failed at block %d\n", blockNum;
      ok = FALSE;
      break;
    }

    off += chunk;
    blockNum++;
    progress1=off;
  }

  if (ok) {
    for (retries = 0; retries < XM_MAX_RETRIES; retries++) {
      UartTxChar(XM_EOT);
      c = UartRxChar();
      if (c == XM_ACK) break;
    }
  }

  Free(data);

  "File transfer finished! %d/%d\n", off, size;
  PressAKey;
  ProgressBarsRst;  

  if(ok) return 0;
  else return -1;
}
