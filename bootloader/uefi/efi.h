#ifndef EFI_H
#define EFI_H

#include <stddef.h>
#include <stdint.h>

#define EFIAPI __attribute__((ms_abi))

typedef uint64_t EFI_STATUS;
typedef uint64_t UINTN;
typedef uint16_t CHAR16;
typedef void *EFI_HANDLE;
typedef void *EFI_EVENT;

#define EFI_SUCCESS 0
#define EFI_INVALID_PARAMETER 2
#define EFI_ERROR_BIT 0x8000000000000000ULL
#define EFI_ERROR(status) (((EFI_STATUS)(status) & EFI_ERROR_BIT) != 0)
#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    { 0x5B1B31A1, 0x9562, 0x11d2, \
      { 0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }

typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} EFI_GUID;

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                               uint8_t ExtendedVerification);
    EFI_STATUS (EFIAPI *OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                      CHAR16 *String);
    EFI_STATUS (EFIAPI *TestString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                    CHAR16 *String);
    EFI_STATUS (EFIAPI *QueryMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                   UINTN ModeNumber, UINTN *Columns, UINTN *Rows);
    EFI_STATUS (EFIAPI *SetMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                 UINTN ModeNumber);
    EFI_STATUS (EFIAPI *SetAttribute)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                      UINTN Attribute);
    EFI_STATUS (EFIAPI *ClearScreen)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
    EFI_STATUS (EFIAPI *SetCursorPosition)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                           UINTN Column, UINTN Row);
    EFI_STATUS (EFIAPI *EnableCursor)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                      uint8_t Visible);
    void *Mode;
};

typedef void (EFIAPI *EFI_EVENT_NOTIFY)(EFI_EVENT Event, void *Context);

typedef struct {
    EFI_TABLE_HEADER Hdr;

    void *RaiseTPL;
    void *RestoreTPL;

    EFI_STATUS (EFIAPI *AllocatePages)(uint32_t Type, uint32_t MemoryType,
                                       UINTN Pages, uint64_t *Memory);
    EFI_STATUS (EFIAPI *FreePages)(uint64_t Memory, UINTN Pages);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *MemoryMapSize, void *MemoryMap,
                                      UINTN *MapKey, UINTN *DescriptorSize,
                                      uint32_t *DescriptorVersion);
    EFI_STATUS (EFIAPI *AllocatePool)(uint32_t PoolType, UINTN Size,
                                      void **Buffer);
    EFI_STATUS (EFIAPI *FreePool)(void *Buffer);

    EFI_STATUS (EFIAPI *CreateEvent)(uint32_t Type, UINTN NotifyTpl,
                                     EFI_EVENT_NOTIFY NotifyFunction,
                                     void *NotifyContext, EFI_EVENT *Event);
    EFI_STATUS (EFIAPI *SetTimer)(EFI_EVENT Event, uint32_t Type,
                                  uint64_t TriggerTime);
    EFI_STATUS (EFIAPI *WaitForEvent)(UINTN NumberOfEvents, EFI_EVENT *Event,
                                      UINTN *Index);
    EFI_STATUS (EFIAPI *SignalEvent)(EFI_EVENT Event);
    EFI_STATUS (EFIAPI *CloseEvent)(EFI_EVENT Event);
    EFI_STATUS (EFIAPI *CheckEvent)(EFI_EVENT Event);

    EFI_STATUS (EFIAPI *InstallProtocolInterface)(EFI_HANDLE *Handle,
                                                  EFI_GUID *Protocol,
                                                  uint32_t InterfaceType,
                                                  void *Interface);
    EFI_STATUS (EFIAPI *ReinstallProtocolInterface)(EFI_HANDLE Handle,
                                                    EFI_GUID *Protocol,
                                                    void *OldInterface,
                                                    void *NewInterface);
    EFI_STATUS (EFIAPI *UninstallProtocolInterface)(EFI_HANDLE Handle,
                                                    EFI_GUID *Protocol,
                                                    void *Interface);
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                        void **Interface);
    void *Reserved;
    EFI_STATUS (EFIAPI *RegisterProtocolNotify)(EFI_GUID *Protocol,
                                                EFI_EVENT Event,
                                                void **Registration);
    EFI_STATUS (EFIAPI *LocateHandle)(uint32_t SearchType, EFI_GUID *Protocol,
                                      void *SearchKey, UINTN *BufferSize,
                                      EFI_HANDLE *Buffer);
    EFI_STATUS (EFIAPI *LocateDevicePath)(EFI_GUID *Protocol, void **DevicePath,
                                          EFI_HANDLE *Device);
    EFI_STATUS (EFIAPI *InstallConfigurationTable)(EFI_GUID *Guid, void *Table);

    EFI_STATUS (EFIAPI *LoadImage)(uint8_t BootPolicy,
                                   EFI_HANDLE ParentImageHandle,
                                   void *DevicePath, void *SourceBuffer,
                                   UINTN SourceSize, EFI_HANDLE *ImageHandle);
    EFI_STATUS (EFIAPI *StartImage)(EFI_HANDLE ImageHandle,
                                    UINTN *ExitDataSize, CHAR16 **ExitData);
    EFI_STATUS (EFIAPI *Exit)(EFI_HANDLE ImageHandle, EFI_STATUS ExitStatus,
                              UINTN ExitDataSize, CHAR16 *ExitData);
    EFI_STATUS (EFIAPI *UnloadImage)(EFI_HANDLE ImageHandle);
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);

    EFI_STATUS (EFIAPI *GetNextMonotonicCount)(uint64_t *Count);
    EFI_STATUS (EFIAPI *Stall)(UINTN Microseconds);
    EFI_STATUS (EFIAPI *SetWatchdogTimer)(UINTN Timeout,
                                          uint64_t WatchdogCode,
                                          UINTN DataSize,
                                          CHAR16 *WatchdogData);

    EFI_STATUS (EFIAPI *ConnectController)(EFI_HANDLE ControllerHandle,
                                           EFI_HANDLE *DriverImageHandle,
                                           void *RemainingDevicePath,
                                           uint8_t Recursive);
    EFI_STATUS (EFIAPI *DisconnectController)(EFI_HANDLE ControllerHandle,
                                              EFI_HANDLE DriverImageHandle,
                                              EFI_HANDLE ChildHandle);

    EFI_STATUS (EFIAPI *OpenProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                      void **Interface, EFI_HANDLE AgentHandle,
                                      EFI_HANDLE ControllerHandle,
                                      uint32_t Attributes);
    EFI_STATUS (EFIAPI *CloseProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                       EFI_HANDLE AgentHandle,
                                       EFI_HANDLE ControllerHandle);

    EFI_STATUS (EFIAPI *OpenProtocolInformation)(EFI_HANDLE Handle,
                                                 EFI_GUID *Protocol,
                                                 void **EntryBuffer,
                                                 UINTN *EntryCount);
    EFI_STATUS (EFIAPI *ProtocolsPerHandle)(EFI_HANDLE Handle,
                                            EFI_GUID ***ProtocolBuffer,
                                            UINTN *ProtocolBufferCount);
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(uint32_t SearchType,
                                            EFI_GUID *Protocol,
                                            void *SearchKey,
                                            UINTN *NoHandles,
                                            EFI_HANDLE **Buffer);

    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *Protocol, void *Registration,
                                        void **Interface);
    EFI_STATUS (EFIAPI *InstallMultipleProtocolInterfaces)(EFI_HANDLE *Handle,
                                                           ...);
    EFI_STATUS (EFIAPI *UninstallMultipleProtocolInterfaces)(EFI_HANDLE Handle,
                                                             ...);
    EFI_STATUS (EFIAPI *CalculateCrc32)(void *Data, UINTN DataSize,
                                        uint32_t *Crc32);

    void *(EFIAPI *CopyMem)(void *Destination, const void *Source, UINTN Length);
    void *(EFIAPI *SetMem)(void *Buffer, UINTN Size, uint8_t Value);
    EFI_STATUS (EFIAPI *CreateEventEx)(uint32_t Type, UINTN NotifyTpl,
                                       EFI_EVENT_NOTIFY NotifyFunction,
                                       const void *NotifyContext,
                                       const EFI_GUID *EventGroup,
                                       EFI_EVENT *Event);
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    uint32_t Revision;
    EFI_HANDLE ParentHandle;
    void *SystemTable;
    EFI_HANDLE DeviceHandle;
    void *FilePath;
    void *Reserved;
    uint32_t LoadOptionsSize;
    void *LoadOptions;
    void *ImageBase;
    uint64_t ImageSize;
    uint32_t ImageCodeType;
    uint32_t ImageDataType;
    void *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    { 0x964E5B22, 0x6459, 0x11d2, \
      { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }

typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
                                    EFI_FILE_PROTOCOL **Root);
};

#define EFI_FILE_MODE_READ 0x0000000000000001ULL

typedef struct {
    uint64_t Size;
    uint64_t FileSize;
    uint64_t PhysicalSize;
    uint64_t CreateTime[2];
    uint64_t LastAccessTime[2];
    uint64_t ModificationTime[2];
    uint64_t Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO;

#define EFI_FILE_INFO_GUID \
    { 0x09576E92, 0x6D3F, 0x11d2, \
      { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }

struct EFI_FILE_PROTOCOL {
    uint64_t Revision;
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL *This,
                              EFI_FILE_PROTOCOL **NewHandle,
                              CHAR16 *FileName, uint64_t OpenMode,
                              uint64_t Attributes);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Delete)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize,
                              void *Buffer);
    EFI_STATUS (EFIAPI *Write)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize,
                               void *Buffer);
    EFI_STATUS (EFIAPI *GetPosition)(EFI_FILE_PROTOCOL *This,
                                     uint64_t *Position);
    EFI_STATUS (EFIAPI *SetPosition)(EFI_FILE_PROTOCOL *This,
                                     uint64_t Position);
    EFI_STATUS (EFIAPI *GetInfo)(EFI_FILE_PROTOCOL *This,
                                 EFI_GUID *InformationType,
                                 UINTN *BufferSize, void *Buffer);
    EFI_STATUS (EFIAPI *SetInfo)(EFI_FILE_PROTOCOL *This,
                                 EFI_GUID *InformationType,
                                 UINTN BufferSize, void *Buffer);
    EFI_STATUS (EFIAPI *Flush)(EFI_FILE_PROTOCOL *This);
};

_Static_assert(sizeof(EFI_TABLE_HEADER) == 24, "UEFI table header size mismatch");
_Static_assert(offsetof(EFI_BOOT_SERVICES, SetWatchdogTimer) == 0x100,
               "SetWatchdogTimer offset mismatch");
_Static_assert(sizeof(EFI_BOOT_SERVICES) == 0x178,
               "BootServices table size mismatch");
_Static_assert(offsetof(EFI_SYSTEM_TABLE, BootServices) == 0x60,
               "SystemTable BootServices offset mismatch");
_Static_assert(offsetof(EFI_LOADED_IMAGE_PROTOCOL, DeviceHandle) == 24,
               "Loaded Image DeviceHandle offset mismatch");
_Static_assert(offsetof(EFI_FILE_PROTOCOL, GetInfo) == 64,
               "File Protocol GetInfo offset mismatch");

#endif