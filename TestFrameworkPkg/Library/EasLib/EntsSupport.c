/** @file
  Ents Support Function implementations.

  Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Library/EntsLib.h>

typedef struct _EFI_NET_ASSERTION_CONFIG {
  BOOLEAN                              Initialized;

  EFI_MAC_ADDRESS                      ServerMac;
  EFI_MAC_ADDRESS                      MacAddr;

  EFI_IPv4_ADDRESS                     ServerIp;
  UINT16                               ServerPort;

  EFI_IPv4_ADDRESS                     StationIp;
  UINT16                               StationPort;

  EFI_HANDLE                           mImageHandle;
  EFI_SERVICE_BINDING_PROTOCOL         *mMnpSb;
  EFI_HANDLE                           mMnpChildHandle;
  EFI_MANAGED_NETWORK_PROTOCOL         *mMnp;

  UINT32                               SequenceId;
  VOID                                 *MessageBuf;
  EFI_MANAGED_NETWORK_COMPLETION_TOKEN TxToken;
} EFI_NET_ASSERTION_CONFIG;

STATIC BOOLEAN                         Finished;

STATIC EFI_NET_ASSERTION_CONFIG        NetAssertionConfigData  = {0, };

#define MAX_DEVICE_PATH_STR_LEN      128
UINTN                          NICDevicePathLen                            = 0;
CHAR16                         NICDevicePathStr[MAX_DEVICE_PATH_STR_LEN]   = {0, };

#define SCT_AGENT_NIC_DEVICE_PATH    L"Sct Agent NIC Device Path"
EFI_GUID gSctVendorGuid = {0x72092b90, 0x17da, 0x47d1, {0x95, 0xce, 0x88, 0xf0, 0x12, 0xe8, 0x50, 0x8d}};

#define MAX_PACKET_LENGTH 1492

STATIC EFI_MANAGED_NETWORK_TRANSMIT_DATA EntsMnpTxDataTemplate = {
  &NetAssertionConfigData.ServerMac,     // DestinationAddress
  &NetAssertionConfigData.MacAddr,       // SourceAddress
  0x0800,                                // ProtocolType
  0,                                     // DataLength
  0,                                     // HeaderLength
  1,                                     // FragmentCount
  {                                      // FragmentTable
    {0, NULL}
  }
};

#define ENTS_SERVER_MAC_ADDRESS_NAME    L"ServerMac"

STATIC EFI_MANAGED_NETWORK_CONFIG_DATA     EntsMnpConfigDataTemplate = {
  0,                      // ReceivedQueueTimeoutValue
  0,                      // TransmitQueueTimeoutValue
  1514,                  // ProtocolTypeFilter
  TRUE,                   // EnableUnicastReceive
  FALSE,                  // EnableMulticastReceive
  TRUE,                   // EnableBroadcastReceive
  FALSE,                  // EnablePromiscuousReceive
  FALSE,                  // EnableReceiveTimestamps
  FALSE                   // DisableBackgroundPolling
};

STATIC
EFI_STATUS
SctGuidToStr (
  IN EFI_GUID                     *Guid,
  OUT CHAR16                      *Buffer
  )
/*++

Routine Description:

  Convert a GUID to a string.

--*/
{
  UINT32  Index;
  UINT32  BufferIndex;
  UINT32  DataIndex;
  UINT32  Len;

  if ((Guid == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  BufferIndex = 0;

  //
  // Convert Guid->Data1
  //
  Len = sizeof(UINT32)*2;
  for (Index = 0; Index < Len; Index++) {
    Buffer[BufferIndex]  = 0;
    Buffer[BufferIndex] = (CHAR16)((Guid->Data1 & (0xf0000000>>(4 * Index)))>>(4 * (Len - Index - 1)));
    if (Buffer[BufferIndex] < 0x0A) {
       Buffer[BufferIndex] += (CHAR16)(L'0');
    } else {
       Buffer[BufferIndex] = (CHAR16)((L'A') + (Buffer[BufferIndex] - 0x0A));
    }
    BufferIndex ++;
  }

  Buffer[BufferIndex++] = L'-';

  //
  // Convert Guid->Data2
  //
  Len = sizeof(UINT16)*2;
  for (Index = 0; Index < Len; Index++) {
    Buffer[BufferIndex]  = 0;
    Buffer[BufferIndex] = (CHAR16)((Guid->Data2 & (0xf000>>(4 * Index)))>>(4 * (Len - Index - 1)));
    if (Buffer[BufferIndex] < 0x0A) {
       Buffer[BufferIndex] += (CHAR16)(L'0');
    } else {
       Buffer[BufferIndex] = (CHAR16)((L'A') + (Buffer[BufferIndex] - 0x0A));
    }
    BufferIndex ++;
  }

  Buffer[BufferIndex++] = L'-';

  //
  // Convert Guid->Data3
  //
  Len = sizeof(UINT16)*2;
  for (Index = 0; Index < Len; Index++) {
    Buffer[BufferIndex]  = 0;
    Buffer[BufferIndex] = (CHAR16)((Guid->Data3 & (0xf000>>(4 * Index)))>>(4 * (Len - Index - 1)));
    if (Buffer[BufferIndex] < 0x0A) {
       Buffer[BufferIndex] += (CHAR16)(L'0');
    } else {
       Buffer[BufferIndex] = (CHAR16)((L'A') + (Buffer[BufferIndex] - 0x0A));
    }
    BufferIndex ++;
  }

  Buffer[BufferIndex++] = L'-';

  //
  // Convert Guid->Data4[x]
  //
  Len = sizeof(UINT8)*2;
  for (DataIndex = 0; DataIndex < 8; DataIndex++) {
     for (Index = 0; Index < Len; Index++) {
        Buffer[BufferIndex]  = 0;
        Buffer[BufferIndex] = (CHAR16)((Guid->Data4[DataIndex] & (0xf0>>(4 * Index)))>>(4 * (Len - Index - 1)));
        if (Buffer[BufferIndex] < 0x0A) {
           Buffer[BufferIndex] += (CHAR16)(L'0');
        } else {
       Buffer[BufferIndex] = (CHAR16)((L'A') + (Buffer[BufferIndex] - 0x0A));
        }
        BufferIndex ++;
     }

     if (DataIndex == 1) {
       Buffer[BufferIndex++] = L'-';
     }
  }

  Buffer[BufferIndex] = 0;

  return EFI_SUCCESS;
}

//
// --------------------------------------String utility-------------------------
//
VOID
Char8StrCpy (
  IN CHAR8   *Dest,
  IN CHAR8   *Src
  )
{
  while (*Src) {
    *(Dest++) = *(Src++);
  }

  *Dest = 0;
}

UINT32
Char8StrLen (
  IN CHAR8   *s1
  )
{
  UINT32  len;

  for (len = 0; *s1; s1 += 1, len += 1)
    ;
  return len;
}

VOID
Char8StrCat (
  IN CHAR8   *Dest,
  IN CHAR8   *Src
  )
{
  Char8StrCpy (Dest + Char8StrLen (Dest), Src);
}

CHAR8 *
Unicode2Ascii (
  OUT CHAR8          *AsciiStr,
  IN  CHAR16         *UnicodeStr
  )
/*++

Routine Description:
  Converts ASCII characters to Unicode.
  Assumes that the Unicode characters are only these defined in the ASCII set.

Arguments:
  AsciiStr   - The ASCII string to be written to. The buffer must be large enough.
  UnicodeStr - the Unicode string to be converted.

Returns:
  The address to the ASCII string - same as AsciiStr.

--*/
{
  while (TRUE) {
    *AsciiStr = (CHAR8) *UnicodeStr++;
    if (*AsciiStr++ == '\0') {
      return AsciiStr;
    }
  }
}
//
// --------------------------------------Computing check sum--------------------
//
UINT16
IpCkSum (
  UINT16 *buf,
  UINT32 nwords
  )
{
  UINT32  sum;
  UINT32  k;

  sum = 0;
  if (nwords < 0) {
    return 0xffff;
  }

  for (k = 0; k < nwords; k++) {
    sum += (UINT16) (*buf++);
    if (sum & 0xFFFF0000) {
      sum = (sum >> 16) + (sum & 0x0000ffff);
    }
  }

  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);

  return HTONS ((UINT16) (~sum));
}

UINT16
UdpCkSum (
  UINT32 srcip,
  UINT32 dstip,
  UINT8  protocol,
  UINT16 udplen,
  UINT16 srcport,
  UINT16 dstport,
  UINT16 *buf,
  UINT32 nwords
  )
{
  UINT32  sum;
  UINT32  k;

  sum = 0;
  if (nwords < 0) {
    return 0XFFFF;
  }

  sum = srcip & 0x0000ffff;

  sum += (srcip >> 16) & 0x0000ffff;
  sum += dstip & 0x0000ffff;
  sum += (dstip >> 16) & 0x0000ffff;
  sum += (protocol << 8) & 0x0000ff00;
  sum += udplen;
  sum += srcport;
  sum += dstport;
  sum += udplen;

  for (k = 0; k < nwords; k++) {
    sum += (UINT16) (*buf++);
    if (sum & 0xFFFF0000) {
      sum = (sum >> 16) + (sum & 0x0000ffff);
    }
  }

  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);

  return HTONS ((UINT16) (~sum));
}

STATIC
UINT32
AssertionPayloadGen (
  IN CHAR8                          *MessageHead,
  IN CHAR8                          *MessageBody,
  OUT CHAR8                         *PacketBuffer
  )
/*++

Routine Description:

  To generate the assertion packet (udp's payload).

Arguments:

  MessageHead   - Pointer to message hander buffer.
  MessageBody   - Pointer to message body buffer.
  PacketBuffer  - Pointer to receive packet buffer.

Returns:

  Size of the packet buffer.

--*/
{
  AsciiSPrint (PacketBuffer, EFI_NET_ASSERTION_MAX_LEN, "|%a|", MessageHead);
  Char8StrCpy (PacketBuffer + Char8StrLen (MessageHead) + 2, MessageBody);

  return Char8StrLen (PacketBuffer);
}

STATIC
UINT32
Ipv4PacketGen (
  IN UINT32 srcadd,
  IN UINT32 dstadd,
  IN UINT8  protocol,
  IN UINT8  tos,
  IN UINT8  ttl,
  IN UINT16 id,
  IN UINT32 dmf,
  IN UINT32 offsethead,
  IN UINT8  *data,
  IN UINT32 datalen,
  OUT UINT8  *buffer
  )
/*++

Routine Description:

  To generate the IP packet (ethernet frame's payload).

Arguments:

  srcadd      - Source ip address.
  dstadd      - Destination ip address.
  protocol    - Protocol field.
  tos         - Type Of Service.
  ttl         - TTL field.
  id          - IP id.
  dmf         - Dmf field.
  offsethead  - Header offset.
  data        - Ip data buffer.
  datalen     - Data buffer length.
  buffer      - Pointer to receive data buffer.

Returns:

  Size of IP packet buffer.

--*/
{
  IPV4_PACKET *ippkt;
  INT32       headlen;

  ippkt           = (IPV4_PACKET *) (buffer);

  ippkt->verhlen  = 0x40 | (23 + 0) / 4;
  headlen         = (ippkt->verhlen & 0x0F) * 4;
  ippkt->tos      = tos;
  ippkt->totallen = HTONS ((UINT16)(20 + datalen));
  ippkt->id       = HTONS (id);

  ippkt->flagoff  = HTONS ((int) (dmf & 0x1) == (int) 0x1 ? IP_FLAG_DF : 0);
  ippkt->ttl      = ttl;
  ippkt->protocol = protocol;
  ippkt->srcadd   = HTONL (srcadd);
  ippkt->dstadd   = HTONL (dstadd);
  CopyMem (&ippkt->data[0], data, datalen);

  ippkt->cksum  = 0;
  ippkt->cksum  = HTONS (IpCkSum ((UINT16 *) (buffer), headlen / 2));

  CopyMem (&ippkt->data[0], data, datalen);

  return datalen + IPV4_HEAD_LEN;
}

STATIC
UINT32
UdpPacketGen (
  IN UINT32 srcip,
  IN UINT32 dstip,
  IN UINT16 srcport,
  IN UINT16 dstport,
  IN UINT8  *data,
  IN UINT32 datalen,
  OUT UINT8  *buffer
  )
/*++

Routine Description:

  To generate the UDP packet (IP frame's payload).

Arguments:

  srcip   - Source ip address.
  dstip   - Destination ip address.
  srcport - Source port.
  dstport - Destination port.
  data    - Data buffer.
  datalen - Data buffer length.
  buffer  - Pointer to receive packet buffer.

Returns:

  Size of the packet buffer.

--*/
{
  UDP_PACKET  *udppkt;

  udppkt            = (UDP_PACKET *) buffer;

  udppkt->udp_src   = HTONS (srcport);
  udppkt->udp_dst   = HTONS (dstport);
  udppkt->udp_len   = HTONS ((UINT16)(datalen + 8));  /* length of UDP data */
  udppkt->udp_cksum = (UINT16) 0;

  CopyMem ((UINT8 *) &udppkt->data[0], data, datalen);
  udppkt->data[datalen] = 0;

  udppkt->udp_cksum = HTONS (
                        UdpCkSum (
                          HTONL (srcip),
                          HTONL (dstip),
                          IP_PROTOCOL_UDP,
                          HTONS ((UINT16)(datalen + UDP_HEAD_LEN)),
                          udppkt->udp_src,
                          udppkt->udp_dst,
                          (UINT16 *) data,
                          (datalen + 1) / 2
                          )
                        );

  return datalen + UDP_HEAD_LEN;
}

UINTN
AssertionPacketGen (
  IN  CHAR8                     *MessageHead,
  IN  CHAR8                     *MessageBody,
  IN  EFI_NET_ASSERTION_CONFIG  *ConfigData,
  OUT CHAR8                     *PacketBuffer
  )
{
  CHAR8                       *AssertionPkt;
  UDP_PACKET                  *UdpPkt;
  IPV4_PACKET                 *IpPkt;
  UINT32                      SrcIpv4Add, DstIpv4Add;
  UINT32                      AssertionPktLen;
  UINT32                      Ipv4Len;
  UINT32                      UpdLen;

  SrcIpv4Add = HTONL (*((UINT32 *) ConfigData->StationIp.Addr));
  DstIpv4Add = HTONL (*((UINT32 *) ConfigData->ServerIp.Addr));

  AssertionPkt  = PacketBuffer + IPV4_HEAD_LEN + UDP_HEAD_LEN;
  UdpPkt        = (UDP_PACKET *) (PacketBuffer + IPV4_HEAD_LEN);
  IpPkt         = (IPV4_PACKET *) (PacketBuffer);

  AssertionPktLen = AssertionPayloadGen (
                      MessageHead,
                      MessageBody,
                      AssertionPkt
                      );

  UpdLen = UdpPacketGen (
            SrcIpv4Add,
            DstIpv4Add,
            ConfigData->StationPort,
            ConfigData->ServerPort,
            (UINT8 *) AssertionPkt,
            AssertionPktLen,
            (UINT8 *) UdpPkt
            );

  Ipv4Len = Ipv4PacketGen (
              SrcIpv4Add,
              DstIpv4Add,
              IP_PROTOCOL_UDP,
              IP_DEFAULT_TOS,
              IP_DEFAULT_TTL,
              IP_DEFAULT_ID,
              0,
              0,
              (UINT8 *) UdpPkt,
              UpdLen,
              (UINT8 *) IpPkt
              );

  return Ipv4Len;
}

STATIC
EFI_STATUS
SendAssertion (
  EFI_NET_ASSERTION_CONFIG            *ConfigData,
  CHAR8                               *Message,
  UINT16                              MessageLen
  )
{
  EFI_STATUS                           Status;
  EFI_MANAGED_NETWORK_COMPLETION_TOKEN *TxToken;
  EFI_MANAGED_NETWORK_TRANSMIT_DATA    TxData;
  UINT8                                *FragmentBuffer;

  if (0 == MessageLen) {
    return EFI_SUCCESS;
  }

  TxToken        = &NetAssertionConfigData.TxToken;

  FragmentBuffer = AllocatePool (MessageLen);
  if(NULL == FragmentBuffer) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"SendAssertion: Allocate FragmentBuffer fail"));
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (&TxData, &EntsMnpTxDataTemplate, sizeof (EFI_MANAGED_NETWORK_TRANSMIT_DATA));
  TxData.DataLength                      = (UINT32) MessageLen;
  TxData.FragmentTable[0].FragmentLength = (UINT32) MessageLen;
  TxData.FragmentTable[0].FragmentBuffer = FragmentBuffer;

  CopyMem (
    (UINT8 *) TxData.FragmentTable[0].FragmentBuffer,
    (CHAR8 *) Message,
    MessageLen
    );

  TxToken->Packet.TxData = &TxData;

  //
  // Ready to send buffer
  //
  NetAssertionConfigData.MessageBuf = FragmentBuffer;
  Finished = FALSE;
  Status  = NetAssertionConfigData.mMnp->Transmit (NetAssertionConfigData.mMnp, TxToken);
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"SendAssertion Mnp->Transmit Error"));
  }

  while(Finished != TRUE) {
    NetAssertionConfigData.mMnp->Poll(NetAssertionConfigData.mMnp);
  }

  FreePool(FragmentBuffer);
  NetAssertionConfigData.MessageBuf = NULL;

  return Status;
}

EFI_STATUS
NetRecordAssertion (
  IN NET_EFI_TEST_ASSERTION   Type,
  IN EFI_GUID                 EventId,
  IN CHAR16                   *Description
  )
/*++

Routine Description:

  Records the test result to network.

Arguments:

  Type          - Test result.
  EventId       - GUID for the checkpoint.
  Description   - Simple description for the checkpoint.

Returns:

  EFI_SUCCESS           - Record the assertion successfully.
  EFI_INVALID_PARAMETER - Invalid Type.
  Others                - Problems when sending assertion packets.

--*/
{
  EFI_STATUS  Status;
  CHAR8       *PacketBuffer;
  CHAR8       MessageHeaderBuffer[NET_ASSERTION_MSG_HEADER_LEN];
  CHAR8       MessageBodyBuffer[NET_ASSERTION_MSG_LEN];
  CHAR16      GuidUniStr[GUID_STRING_LEN];
  CHAR8       TypeStr[TYPE_STRING_LEN];
  CHAR8       GuidStr[GUID_STRING_LEN];
  CHAR8       ResultStr[RESULT_STRING_LEN];
  UINT16      MessageLength;

  if (NetAssertionConfigData.Initialized == FALSE) {
    return EFI_SUCCESS;
  }

  PacketBuffer  = NULL;
  Status        = EFI_SUCCESS;

  if (StrLen (Description) > NET_ASSERTION_MSG_LEN) {
    Status = EFI_INVALID_PARAMETER;
  }

  //
  // Fill out MessageHeaderBuffer with the following format:
  // IpAddress String|Guild String|Result String
  // For example:
  // 00:C0:45:00:00:02|F2536CE9-24BE-49ed-8B29-754386AD5BF8|PASS
  //

  SctGuidToStr (&EventId, GuidUniStr);

  Unicode2Ascii (GuidStr, GuidUniStr);
  switch (Type) {
  case NET_EFI_TEST_ASSERTION_PASSED:
    Char8StrCpy (TypeStr, EFI_NET_ASSERTION_TYPE_ASSERTION);
    Char8StrCpy (ResultStr, EFI_NET_ASSERTION_RESULT_PASS);
    break;

  case NET_EFI_TEST_ASSERTION_WARNING:
    Char8StrCpy (TypeStr, EFI_NET_ASSERTION_TYPE_ASSERTION);
    Char8StrCpy (ResultStr, EFI_NET_ASSERTION_RESULT_WARN);
    break;

  case NET_EFI_TEST_ASSERTION_FAILED:
    Char8StrCpy (TypeStr, EFI_NET_ASSERTION_TYPE_ASSERTION);
    Char8StrCpy (ResultStr, EFI_NET_ASSERTION_RESULT_FAIL);
    break;

  case NET_EFI_TEST_ASSERTION_CASE_BEGIN:
    Char8StrCpy (TypeStr, EFI_NET_ASSERTION_TYPE_CASE_BEGN);
    Char8StrCpy (ResultStr, "BEGN");
    break;

  case NET_EFI_TEST_ASSERTION_CASE_OVER:
    Char8StrCpy (TypeStr, EFI_NET_ASSERTION_TYPE_CASE_OVER);
    Char8StrCpy (ResultStr, "OVER");
    break;

  default:
    Status = EFI_INVALID_PARAMETER;
    return Status;
  }

  //
  // Gen the packet sent to SCT OS management side
  //
  PacketBuffer = AllocatePool (EFI_NET_ASSERTION_MAX_LEN);
  if (NULL == PacketBuffer) {
    return EFI_OUT_OF_RESOURCES;
  }

  AsciiSPrint (
    MessageHeaderBuffer,
    NET_ASSERTION_MSG_HEADER_LEN,
    "%a|%a|%a|%a",
#if 0
    NetAssertionConfigData.MacAddr.Addr[0],
    NetAssertionConfigData.MacAddr.Addr[1],
    NetAssertionConfigData.MacAddr.Addr[2],
    NetAssertionConfigData.MacAddr.Addr[3],
    NetAssertionConfigData.MacAddr.Addr[4],
    NetAssertionConfigData.MacAddr.Addr[5],
#else
    NetAssertionConfigData.MacAddr.Addr,
#endif
    TypeStr,
    GuidStr,
    ResultStr
    );

  Unicode2Ascii (MessageBodyBuffer, Description);
  MessageLength = (UINT16) AssertionPacketGen (
                             MessageHeaderBuffer,
                             MessageBodyBuffer,
                             &NetAssertionConfigData,
                             PacketBuffer
                             );
  Status = SendAssertion (&NetAssertionConfigData, PacketBuffer, MessageLength);
  if (EFI_ERROR(Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"%a send assertion fail\n", NetAssertionConfigData.MacAddr));
  }
  return EFI_SUCCESS;
}

STATIC
VOID
EFIAPI
NotifyFunc (
  EFI_EVENT               Event,
  VOID                    *FinishedParameter
  )
{
  *(BOOLEAN *)FinishedParameter = TRUE;
  return ;
}

STATIC
EFI_STATUS
NetAssertionUtilityStart (
  VOID
  )
{
  EFI_STATUS                        Status;
  EFI_SIMPLE_NETWORK_MODE           SnpModeData;
  EFI_MANAGED_NETWORK_PROTOCOL      *Mnp;

  Mnp = NetAssertionConfigData.mMnp;

  Status = Mnp->Configure (
                  Mnp,
                  &EntsMnpConfigDataTemplate
                  );
  if (EFI_ERROR(Status)) {
  EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"NetAssertionUtilityStart Mnp->Configure fail - %r", Status));
    return Status;
  }

  Status = Mnp->GetModeData (
                  Mnp,
                  NULL,
                  &SnpModeData
                  );
  if (EFI_ERROR (Status) && (Status != EFI_NOT_STARTED)) {
  EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"NetAssertionUtilityStart Mnp->GetModeData fail - %r", Status));
    return Status;
  }

  CopyMem (&NetAssertionConfigData.MacAddr, &SnpModeData.CurrentAddress, sizeof (EFI_MAC_ADDRESS));
  EFI_ENTS_DEBUG ((EFI_ENTS_D_TRACE, L"NetAssertionConfigData.MacAddr : %02x:%02x:%02x:%02x:%02x:%02x",
    NetAssertionConfigData.MacAddr.Addr[0], NetAssertionConfigData.MacAddr.Addr[1],
    NetAssertionConfigData.MacAddr.Addr[2], NetAssertionConfigData.MacAddr.Addr[3],
    NetAssertionConfigData.MacAddr.Addr[4], NetAssertionConfigData.MacAddr.Addr[5]));

  NetAssertionConfigData.SequenceId   = 0;
  NetAssertionConfigData.MessageBuf   = NULL;
  Status = gntBS->CreateEvent (
                    EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    NotifyFunc,
                    &Finished,
                    &NetAssertionConfigData.TxToken.Event
                    );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"NetAssertionUtilityStart CreateEvent fail - %r", Status));
    gntBS->CloseEvent (NetAssertionConfigData.TxToken.Event);
    return Status;
  }

  return Status;
}

EFI_STATUS
NetAssertionUtilityInstall (
  )
{
  EFI_STATUS                                Status;
  EFI_SERVICE_BINDING_PROTOCOL              *MnpSb;
  EFI_HANDLE                                MnpChildHandle;
  EFI_HANDLE                                ControllerHandle;
  EFI_MANAGED_NETWORK_PROTOCOL              *Mnp;
  UINTN                                     DataSize;

  //
  // Initialize the NetAssertionConfigData with raw data first
  //
  SetMem ((VOID *) &NetAssertionConfigData, sizeof (EFI_NET_ASSERTION_CONFIG), 0);
  NetAssertionConfigData.Initialized = FALSE;

  NetAssertionConfigData.mImageHandle = mImageHandle;

  DataSize = sizeof(EFI_MAC_ADDRESS);
  Status = GetContextRecord(
              gntDevicePath,
              SCT_PASSIVE_MODE_RECORD_FILE,
              ENTS_SERVER_MAC_ADDRESS_NAME,
              &DataSize,
              &NetAssertionConfigData.ServerMac
              );
  if (EFI_ERROR(Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_WARNING, L"NetAssertionUtilityInstall: GetContextRecord %s fail - %r", ENTS_SERVER_MAC_ADDRESS_NAME, Status));
    return Status;
  }
  SetMem ((VOID *) &NetAssertionConfigData.ServerIp, sizeof (EFI_IPv4_ADDRESS), 0xff);
  NetAssertionConfigData.ServerPort   = 1514;
  NetAssertionConfigData.StationPort  = 1514;

  //
  // Get the wanted Mnp service binding protocol
  //
  Status = EntsNetworkServiceBindingGetControllerHandle (
             &gEfiManagedNetworkServiceBindingProtocolGuid,
             &ControllerHandle
             );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = gntBS->HandleProtocol (
                    ControllerHandle,
                    &gEfiManagedNetworkServiceBindingProtocolGuid,
                    (VOID **)&MnpSb
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Create MNP instance
  //
  MnpChildHandle = 0;
  Status = MnpSb->CreateChild (
                    MnpSb,
                    &MnpChildHandle
                    );
  if (EFI_ERROR (Status)) {
  EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"NetAssertionUtilityInstall CreateChild fail - %r", Status));
    return Status;
  }

  //
  // Open the ManagedNetwork Protocol from ChildHandle
  //
  Status = gntBS->OpenProtocol (
                    MnpChildHandle,
                    &gEfiManagedNetworkProtocolGuid,
                    (VOID **)&Mnp,
                    mImageHandle,
                    MnpChildHandle,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"NetAssertionUtilityInstall OpenProtocol fail - %r", Status));
    goto InitError;
  }

  NetAssertionConfigData.mMnpSb          = MnpSb;
  NetAssertionConfigData.mMnpChildHandle = MnpChildHandle;
  NetAssertionConfigData.mMnp            = Mnp;

  //
  // Start the NetAssertionUtility
  //
  Status = NetAssertionUtilityStart ();
  if (EFI_ERROR (Status)) {
    goto InitError;
  }

  NetAssertionConfigData.Initialized    = TRUE;

  return EFI_SUCCESS;
InitError:
  MnpSb->DestroyChild (
           MnpSb,
           MnpChildHandle
           );
  NetAssertionConfigData.mMnpSb          = NULL;
  NetAssertionConfigData.mMnpChildHandle = NULL;
  NetAssertionConfigData.mMnp            = NULL;
  return Status;
}

VOID
NetAssertionUtilityUninstall (
  VOID
  )
{
  EFI_STATUS                                Status;

  if (NetAssertionConfigData.Initialized != TRUE) {
    return;
  }

  Status = gntBS->CloseProtocol (
                    NetAssertionConfigData.mMnpChildHandle,
                    &gEfiManagedNetworkProtocolGuid,
                    NetAssertionConfigData.mImageHandle,
                    NetAssertionConfigData.mMnpChildHandle
                    );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"NetAssertionUtilityUninstall: CloseProtocol fail - %r", Status));
  }
  NetAssertionConfigData.mMnp         = NULL;

  Status = gntBS->CloseEvent (NetAssertionConfigData.TxToken.Event);
  if (!EFI_ERROR(Status)) {
    if (NetAssertionConfigData.MessageBuf != NULL) {
      FreePool(NetAssertionConfigData.MessageBuf);
      NetAssertionConfigData.MessageBuf = NULL;
    }
  } else {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"NetAssertionUtilityUninstall: gBS->CloseEvent fail - %r", Status));
  }

  Status = NetAssertionConfigData.mMnpSb->DestroyChild (
                                            NetAssertionConfigData.mMnpSb,
                                            NetAssertionConfigData.mMnpChildHandle
                                            );
  if (EFI_ERROR(Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"NetAssertionUtilityUninstall: mMnpSb->DestroyChild fail - %r", Status));
  }
  NetAssertionConfigData.mMnpSb       = NULL;

  NetAssertionConfigData.Initialized  = FALSE;
}

EFI_STATUS
EntsNetworkServiceBindingGetControllerHandle (
  IN  EFI_GUID                 *ProtocolGuid,
  OUT EFI_HANDLE               *ClientHandle
  )
{
  EFI_STATUS                    Status;
  UINTN                         NICDPathLen;
  CHAR16                        NICDPathStr[MAX_DEVICE_PATH_STR_LEN];
  CHAR16                        *TempStr;
  UINTN                         NoHandles;
  UINTN                         Index;
  EFI_HANDLE                    *NICHandleBuffer;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;
  BOOLEAN                       Found;

  NICDPathLen = 0;
  TempStr     = NULL;
  Found       = FALSE;

  NICDPathLen = MAX_DEVICE_PATH_STR_LEN;
  SetMem (NICDPathStr, sizeof(NICDPathStr), 0);
  Status = GetContextRecord(
             gntDevicePath,
             SCT_PASSIVE_MODE_RECORD_FILE,
             SCT_AGENT_NIC_DEVICE_PATH,
             &NICDPathLen,
             NICDPathStr
             );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"Can not get the saved controller device path - %r\n", Status));
    return EFI_SUCCESS;
  }

  EFI_ENTS_DEBUG ((EFI_ENTS_D_TRACE, L"Get the saved NIC device path: %s\n", NICDPathStr));

  Status = gntBS->LocateHandleBuffer(
                    ByProtocol,
                    ProtocolGuid,
            NULL,
                    &NoHandles,
            &NICHandleBuffer
                );
  if (EFI_ERROR(Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"Can not get the NIC handles with ProtocolGuid\n"));
  goto GetClientHandleReturn;
  }

  for (Index = 0; Index < NoHandles; Index++) {
    Status = gntBS->HandleProtocol (
                      NICHandleBuffer[Index],
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&DevicePath
                      );
    if (EFI_ERROR(Status)) {
      EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"Can not get device path - %r\n", Status));
    goto GetClientHandleReturn;
    }
  TempStr = EntsDevicePathToStr(DevicePath);
  if (StrCmp (TempStr, NICDPathStr) == 0) {
    Found = TRUE;
      break;
    }
    gntBS->FreePool (TempStr);
  }

  if (Found == TRUE) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_TRACE, L"Get the NIC device handle\n"));
    *ClientHandle = NICHandleBuffer[Index];
  Status = EFI_SUCCESS;
  } else {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_TRACE, L"Can not get the NIC device handle\n"));
    *ClientHandle = NICHandleBuffer[Index];
  Status = EFI_NOT_FOUND;
  }

GetClientHandleReturn:
  gntBS->FreePool (TempStr);
  gntBS->FreePool (NICHandleBuffer);
  return Status;
}

VOID
EntsChooseNICAndSave (
  VOID
  )
{
  EFI_STATUS                    Status;
  UINTN                         NoHandles;
  UINTN                         Index;
  EFI_HANDLE                    *NicHandleBuffer;
  EFI_DEVICE_PATH_PROTOCOL      *NicDevicePath;
  CHAR16                        *NicDevicePathStr[0x20];
  EFI_INPUT_KEY                 Key;

  SetMem (NicDevicePathStr, sizeof(NicDevicePathStr), 0);

  //
  // Delete NIC device path variable
  //
  Status = SetContextRecord (
             gntDevicePath,
             SCT_PASSIVE_MODE_RECORD_FILE,
             SCT_AGENT_NIC_DEVICE_PATH,
             0,
             NULL
             );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_WARNING, L"EntsChooseNICAndSave: SetContextRecord fail - %r", Status));
  }

  //
  // Specify the NIC and save it's device path
  //
  NoHandles = 0;
  Status = gntBS->LocateHandleBuffer(
                    ByProtocol,
                    &gEfiSimpleNetworkProtocolGuid,
            NULL,
                    &NoHandles,
            &NicHandleBuffer
                );
  if (EFI_ERROR(Status) || NoHandles == 0) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"EntsChooseNICAndSave: LocateHandleBuffer fail - %r", Status));
  return;
  }

  for (Index = 0; Index < NoHandles; Index++) {
    Status = gntBS->HandleProtocol (
                      NicHandleBuffer[Index],
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&NicDevicePath
                      );
    if (EFI_ERROR(Status)) {
      EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"EntsChooseNICAndSave: HandleProtocol fail - %r", NicHandleBuffer[Index], Status));
    goto FreeAndReturn;
    }
    NicDevicePathStr[Index] = EntsDevicePathToStr(NicDevicePath);
  EntsPrint(L"[%d] %s\n", Index, NicDevicePathStr[Index]);
  }

  EntsPrint(L"Please choose a NIC:[0]-[%d]", NoHandles - 1);
  for (;;) {
    EntsWaitForSingleEvent (gntST->ConIn->WaitForKey, 0);
    gntST->ConIn->ReadKeyStroke (gntST->ConIn, &Key);
  Index = Key.UnicodeChar - L'0';
  if (Index < NoHandles) {
      break;
    }
  }
  EntsPrint(L"\n");

  EFI_ENTS_DEBUG ((EFI_ENTS_D_TRACE, L"EntsChooseNICAndSave: Save NIC device path: %s", NicDevicePathStr[Index]));

  Status = SetContextRecord(
             gntDevicePath,
             SCT_PASSIVE_MODE_RECORD_FILE,
             SCT_AGENT_NIC_DEVICE_PATH,
             StrLen(NicDevicePathStr[Index]) * 2,
             NicDevicePathStr[Index]
             );
  if (EFI_ERROR(Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"EntsChooseNICAndSave: SetContextRecord fail - %r", Status));
  goto FreeAndReturn;
  }

FreeAndReturn:
  for (Index = 0; Index < 0x20; Index++) {
    if (NicDevicePathStr[Index] != NULL) {
       gntBS->FreePool(NicDevicePathStr[Index]);
    }
  }
  gntBS->FreePool(NicHandleBuffer);
  return;
}

EFI_STATUS
GetMacAddress (
  OUT UINT8                         *MacAddr
  )
{
  EFI_STATUS                        Status;
  EFI_HANDLE                        ControllerHandle;
  EFI_HANDLE                        MnpInstanceHandle;
  EFI_SERVICE_BINDING_PROTOCOL      *MnpSb;
  EFI_MANAGED_NETWORK_PROTOCOL      *Mnp;
  EFI_SIMPLE_NETWORK_MODE           SnpModeData;

  ZeroMem(MacAddr, sizeof(EFI_MAC_ADDRESS));

  Status = EntsNetworkServiceBindingGetControllerHandle(
             &gEfiManagedNetworkServiceBindingProtocolGuid,
             &ControllerHandle
             );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"GetMacAddress: EntsNetworkServiceBindingGetControllerHandle fail - %r", Status));
    return Status;
  }

  Status = gntBS->HandleProtocol (
                    ControllerHandle,
                    &gEfiManagedNetworkServiceBindingProtocolGuid,
                    (VOID **)&MnpSb
                    );
  if (EFI_ERROR(Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"GetMacAddress: Handle MNP Service Binding Protocol fail - %r", Status));
    return Status;
  }

  MnpInstanceHandle = NULL;
  Status = MnpSb->CreateChild (MnpSb, &MnpInstanceHandle);
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"GetMacAddress: MnpSb->CreateChild error ! - %r", Status));
    return Status;
  }

  Status = gntBS->OpenProtocol (
                    MnpInstanceHandle,
                    &gEfiManagedNetworkProtocolGuid,
                    (VOID **) &Mnp,
                    mImageHandle,
                    MnpInstanceHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"GetMacAddress: OpenProtocol error ! - %r\n", Status));
    return Status;
  }

  Status = Mnp->Configure(Mnp, &EntsMnpConfigDataTemplate);
  if (EFI_ERROR(Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"GetMacAddress: Mnp->Configure fail - %r", Status));
    return Status;
  }
  Status = Mnp->GetModeData(Mnp, NULL, &SnpModeData);
  if (EFI_ERROR(Status)) {
    EFI_ENTS_DEBUG((EFI_ENTS_D_ERROR, L"GetMacAddress: Mnp->GetModeData fail - %r", Status));
    return Status;
  }
  CopyMem (MacAddr, &SnpModeData.CurrentAddress, sizeof (EFI_MAC_ADDRESS));

  Status = gntBS->CloseProtocol (
                    MnpInstanceHandle,
                    &gEfiManagedNetworkProtocolGuid,
                    mImageHandle,
                    MnpInstanceHandle
                    );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"GetMacAddress: CloseProtocol error ! - %r\n", Status));
    return Status;
  }

  Status = MnpSb->DestroyChild (MnpSb, MnpInstanceHandle);
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"GetMacAddress: DestroyChild error ! - %r", Status));
    return Status;
  }

  return EFI_SUCCESS;
}
