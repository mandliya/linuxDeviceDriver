#define Device_VRAM 0x0020
#define Device_FIFOSize 0x002c
#define Config_Reboot 0x1000
#define Config_ModeSet 0x1008
#define Config_Interrupt 0x100c
#define Config_Accel 0x1010
#define Buffer_Address 0x2000
#define Buffer_Config 0x2008
#define Raster_Prim 0x3000
#define Raster_Emit 0x3004
#define Raster_Clear 0x3008
#define Raster_Flush 0x3ffc
#define FIFO_Depth 0x4004
#define Info_Status 0x4008
#define Vertex_X 0x5000
#define Vertex_Y 0x5004
#define Vertex_Z 0x5008
#define Vertex_W 0x500C 
#define Vertex_R 0x5010
#define Vertex_G 0x5014
#define Vertex_B 0x5018
#define Vertex_A 0x501C 
#define Clear_R 0x5100
#define Clear_G 0x5104
#define Clear_B 0x5108
#define Frame_Col 0x8000
#define Frame_Row 0x8004
#define Frame_Pitch 0x8008
#define Frame_Pixel 0x800c
#define Frame_Start 0x8010
#define Encoder_Width 0x9000
#define Encoder_Height 0x9004
#define Encoder_OffX 0x9008
#define Encoder_OffY 0x900c
#define Encoder_Frame 0x9010

#define VMODE _IOW(0xCC, 0, unsigned long)
#define SYNC  _IO(0xCC, 3)
#define BIND_DMA _IOW(0xCC, 1, unsigned long)
#define START_DMA _IOWR(0xCC, 2, unsigned long)
#define SET_SIZE _IOW(0xCC, 5, unsigned long)
#define FLUSH _IO(0xCC, 4)

#define BUFFER_SIZE 124
#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0
