/*
 * ################################################################
   File: tester.c 
   Purpose: User code to test the Kyouko2 graphic card driver.
   Use: Gives option to test DMA and FIFO facilty, FIFO is allowed
	only for root user.
   Team: Ben Whetstone, Ravi Mandliya
   ################################################################
*/

//header files
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <time.h>

//header file defining the device registers.
#include "defs.h"

#define SIZE_BUFFER 124
#define SCALE 1024
#define KYOUKO2_CONTROL_SIZE (65536)
#define Device_RAM (0x0020)

//USER Struct: This structure holds the addresss of control base and frame buffer
struct u_kyouko2_device{
  unsigned int *u_control_base;
  unsigned int *u_fb_base;
}k2;

//USER Struct: This structure holds the header for DMA command. 

struct kyouko2_dma_header{
  uint32_t stride:5;
  uint32_t has_v4:1;
  uint32_t has_c3:1;
  uint32_t has_c4:1;

  uint32_t unused:4;

  uint32_t prim_type:2;
  uint32_t count:10;
  uint32_t opcode:8;
};

//USER Union: This is a union for holding commands and values for DMA buffer
union buffer{
  struct kyouko2_dma_header header;
  float memory;
};

//USER Array holding position of 4 equilateral triangles.
float position[4][3][3] = {
	{
		{-0.66,-0.33,0.0},
		{-0.50,-0.615,0.0},
		{-0.33,-0.33,0.0}
  	},
	{
		{0.33,0.33,0.0},
		{0.50,0.615,0.0},
		{0.66, 0.33,0.0}

	},
	{
		{-0.66, 0.33,0.0},
		{-0.50,0.615,0.0},
		{-0.33,0.33,0.0}
	},
	{
		{0.33,-0.33,0.0},
		{0.50,-0.615,0.0},
		{0.66, -0.33,0.0}
	}
        
};
//USER an array containing RGB values
float color[3][3] = {
	{1.0,0.0,0.0},
	{0.0,1.0,0.0},
	{0.0,0.0,1.0}
};



//To read a register value
unsigned int U_READ_REG( unsigned int regist){
  return (*(k2.u_control_base+(regist>>2)));
}

//To write to a register
void U_WRITE_REG(unsigned int reg, unsigned int val) {
  *(k2.u_control_base+(reg>>2))= val; 
}

//To write a float value to a register
void U_WRITE_REG_F(unsigned int reg, float val) {
  *(k2.u_control_base+(reg>>2))= *(unsigned int *) (&val);
}

//To write to a frame buffer
void U_WRITE_FB(unsigned int off, unsigned int val) {
  *(k2.u_fb_base+off)= val; 
}


//A function to fill the FIFO commands 
void fillFifoReg(int fd){
 
  U_WRITE_REG(Raster_Clear, 1);

  U_WRITE_REG(Raster_Prim, 1);
  
//Red Vertex
  U_WRITE_REG_F(Vertex_X, -0.5);
  U_WRITE_REG_F(Vertex_Y, -0.5);
  U_WRITE_REG_F(Vertex_Z, 0.0);
  U_WRITE_REG_F(Vertex_W, 1.0);

  U_WRITE_REG_F(Vertex_R, 1.0);
  U_WRITE_REG_F(Vertex_G, 0.0);
  U_WRITE_REG_F(Vertex_B, 0.0);
  U_WRITE_REG_F(Vertex_A, 0.0);

//Emit
  U_WRITE_REG(Raster_Emit, 0);


//Green Vertex
  U_WRITE_REG_F(Vertex_X, 0.5);
  U_WRITE_REG_F(Vertex_Y, 0.0);
  U_WRITE_REG_F(Vertex_Z, 0.0);
  U_WRITE_REG_F(Vertex_W, 1.0);

  U_WRITE_REG_F(Vertex_R, 0.0);
  U_WRITE_REG_F(Vertex_G, 1.0);
  U_WRITE_REG_F(Vertex_B, 0.0);
  U_WRITE_REG_F(Vertex_A, 0.0);

  U_WRITE_REG(Raster_Emit, 0);


//blue vertex
  U_WRITE_REG_F(Vertex_X, 0.125);
  U_WRITE_REG_F(Vertex_Y, 0.5);
  U_WRITE_REG_F(Vertex_Z, 0.0);
  U_WRITE_REG_F(Vertex_W, 1.0);

  U_WRITE_REG_F(Vertex_R, 0.0);
  U_WRITE_REG_F(Vertex_G, 0.0);
  U_WRITE_REG_F(Vertex_B, 1.0);
  U_WRITE_REG_F(Vertex_A, 0.0);

  U_WRITE_REG(Raster_Emit, 0);

  ioctl(fd, SYNC);

  U_WRITE_REG(Raster_Prim, 0);
  U_WRITE_REG(Raster_Flush, 0);
}


//To genrate random float value between 0 and 1
float rand_float(void) {
  return (float)rand()/(float)RAND_MAX;
}


//To generate random float value in a range
float rand_range(float min,float max){
 float scaled =(float)rand()/RAND_MAX;
 return (max-min+1.0)*scaled+min;
}


//To fill in a random triangle to the buffer
void rand_tri(union buffer* buffer1) {

  union buffer tri_header;
  tri_header.header.stride=5;
  tri_header.header.has_v4=0;
  tri_header.header.has_c3=1;
  tri_header.header.has_c4=0;
  tri_header.header.prim_type=1;
  tri_header.header.count=3;
  tri_header.header.opcode=0x14;


  buffer1[0].header=tri_header.header;
  
  buffer1[1].memory=rand_float();
  buffer1[2].memory=rand_float();
  buffer1[3].memory=rand_float();
  buffer1[4].memory=rand_range(-1,0.5);
  buffer1[5].memory=rand_range(-1,0.5);
  buffer1[6].memory=rand_range(-1,0.5);
  buffer1[7].memory=rand_float();
  buffer1[8].memory=rand_float();
  buffer1[9].memory=rand_float();
  buffer1[10].memory=rand_range(-0.5,0.5);
  buffer1[11].memory=rand_range(-0.5,0.5);
  buffer1[12].memory=rand_range(-0.5,0.5);
  buffer1[13].memory=rand_float();
  buffer1[14].memory=rand_float();
  buffer1[15].memory=rand_float();
  buffer1[16].memory=rand_range(-0.5,0.5);
  buffer1[17].memory=rand_range(-0.5,0.5);
  buffer1[18].memory=rand_range(-0.5,0.5);
}


//Filling in the DMA header to buffer
void fillHeader(union buffer tri_header){

  tri_header.header.stride=5;
  tri_header.header.has_v4=0;
  tri_header.header.has_c3=1;
  tri_header.header.has_c4=0;
  tri_header.header.prim_type=1;
  tri_header.header.count=3;
  tri_header.header.opcode=0x14;
 
}

//Draw 4 equilateral triangles
void drawTriangles(union buffer *buff, int pIndex){
  int cIndex = 0, i, j;
  union buffer tri_header;
  tri_header.header.stride=5;
  tri_header.header.has_v4=0;
  tri_header.header.has_c3=1;
  tri_header.header.has_c4=0;
  tri_header.header.prim_type=1;
  tri_header.header.count=3;
  tri_header.header.opcode=0x14;
 buff[cIndex++].header=tri_header.header;
  for(i = 0; i< 3; ++i){
     for(j =0; j < 3; ++j){
   	  buff[cIndex++].memory = color[i][j];
     }
     for(j = 0; j < 3; ++j){
   	  buff[cIndex++].memory = position[pIndex][i][j];
     }
  }
}



int main(){

  int fd,i, FB_size;
  unsigned int *arg = NULL;
  union buffer* buffer1;
  unsigned int buffsize=19*sizeof(float);
  srand(time(NULL));
  fd = open("/dev/kyouko2", O_RDWR);
  for(i=0; i < 30; ++i)
	printf("\n");
  int choice;

  printf("Enter choice:\n1.Draw Triangle using FIFO\n2.Draw Triangle using DMA\nYour choice is:");
  scanf("%d",&choice);
  switch(choice){
    //CASE to use FIFO
    case 1:
	{
                 //Getting a user address for Control Register
		k2.u_control_base = mmap(0, KYOUKO2_CONTROL_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if((long)k2.u_control_base == -1)
      break;
		FB_size = U_READ_REG(Device_RAM);
                //Getting a user address for Frame Buffer
k2.u_fb_base = mmap(0, FB_size*1024*1024, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x80000000);
	
  		ioctl(fd, VMODE, GRAPHICS_ON);
	  	ioctl(fd, SYNC);
		fillFifoReg(fd);
		sleep(5);
         	ioctl(fd, VMODE, GRAPHICS_OFF);
		    break;

	}
    //CASE for using DMA
    case 2:
	{
		int choice2;
		printf("1.Draw 100 random triangles\n2.Draw 4 equilateral triangles\nYour choice:");
		scanf("%d",&choice2);
		switch(choice2){	
                //Internal CASE for drawing 100 random triangles using DMA
 		case 1:
		{
			int k;
	 		ioctl(fd, VMODE, GRAPHICS_ON);
			ioctl(fd, SYNC);
    		        ioctl(fd, BIND_DMA, &arg);
		        for(k = 0; k < 100; ++k){
			        buffer1 = *(union buffer**)&arg;
		        	rand_tri(buffer1);
				arg=(*(unsigned int*)&(buffsize));
				ioctl(fd, START_DMA, &arg);
				ioctl(fd, FLUSH);
 			}
 			sleep(5);
			ioctl(fd, VMODE, GRAPHICS_OFF);
			break;

		}
          
                //Internal case for drawing 4 equilateral triangle using DMA
		case 2:
		{
	 	        ioctl(fd, VMODE, GRAPHICS_ON);
			ioctl(fd, SYNC);
		        ioctl(fd, BIND_DMA, &buffer1);
	 		for(i = 0; i < 4 ; ++i){
  		        	drawTriangles(buffer1, i);
				arg = *(unsigned int*)&buffsize;
 				ioctl(fd, START_DMA, &arg);
	 			ioctl(fd, FLUSH);
                         	buffer1 = (union buffer*) arg;
			}	
		        sleep(3);
			ioctl(fd, VMODE, GRAPHICS_OFF);
			break;
 		}
		default:
			printf("Invalid Choice\n");
			break;
               }
	  break;
    }
   default:
		printf("Invalid Option\n");
		break;
 }
  close(fd);
  return 0;
}
