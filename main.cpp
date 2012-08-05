#include <cstdio>
#include <cstring>
#include <vector>
#include <iostream>
#include "SDL.h" 
#include <ctime>

typedef Uint8 BYTE; 
typedef Uint16 WORD; 
typedef bool BIT; 
typedef std::vector<WORD> stack_w; 

// The Hardware
BYTE gmem[0xFFF];       // 0xFFF bytes of memory 2^12
BYTE reg[16];           // 16 registers, 1 byte each 
WORD addrI;             // the 16-bit address register
WORD pc;                // the 16-bit program counter
stack_w stack;          // the 16-bit stack
BIT disp[64][32];      // 2D array 
bool keyboard[16]; 
BYTE d_timer; 
BYTE s_timer; 
SDL_Surface *screen; 

enum { 
  XMASK = 0x0F00,
  YMASK = 0x00F0
}; 

BYTE chip8_fontset[80] =
{ 
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/* This function resets memory and program counter
 * It then loads game data into memory
 */
inline void resetCPU(const char *file)
{
  addrI = 0; 
  pc = 0x200; 
  std::memset(reg, 0, sizeof(reg)); // set registers to 0 
  std::memset(disp, 0, sizeof(disp)); // set display to 0
  std::memset(keyboard, 0, sizeof(keyboard)); 
  d_timer = 0; 
  s_timer = 0; 
  // Load the game into memory 
  FILE *in; 
  in = fopen(file, "rb"); 
  fread(&gmem[0x200], 0xfff, 1, in); 
  fclose(in); 
}

/* This function takes the current place in memory 
 * and logically combines with the next one, 
 * returning the result. This is to compenstate for 2byte opcodes 
 * with only 1 byte located in memory
 */
inline WORD getNextOp() 
{ 
  WORD res = (gmem[pc] << 8) | gmem[pc+1];
  pc += 2; 
  return res; 
}

/* THE OPCODE FUNCTIONS! 
 */

// Clear the screen
inline void op00E0(WORD op) { 
  memset(disp, 0, sizeof(disp)); 
}

// Return from a subroutine
inline void op00EE(WORD op) { 
  pc = stack.back();
  stack.pop_back();
}

// Jump to address
inline void op1NNN(WORD op) {
  pc = op & 0x0FFF; 
}

// Call subroutine at address
inline void op2NNN(WORD op) { 
  stack.push_back(pc); 
  pc = op & 0x0FFF; 
}

// Skip the next instruction if VX equals NN
inline void op3XNN(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int nn = op & 0x00FF; 
  if(reg[x] == nn) pc += 2; 
}

// Skip the next instruction if VX doesn't equal NN
inline void op4XNN(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int nn = op & 0x00FF; 
  if(reg[x] != nn) pc += 2; 
}

// Skip the next instruction if VX equals VY
inline void op5XY0(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>= 4; 
  if(reg[x] == reg[y]) pc +=2; 
}

// Set VX to NN
inline void op6XNN(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int nn = op & 0x00FF; 
  reg[x] = nn; 
}

// Add NN to VX
inline void op7XNN(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int nn = op & 0x00FF; 

  reg[x] += nn; 
}

// Set VX to VY
inline void op8XY0(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>= 4; 

  reg[x] = reg[y]; 
}

// Set VX to VX | VY
inline void op8XY1(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>= 4; 

  reg[x] |= reg[y]; 
}

// Set VX to VX & VY
inline void op8XY2(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>= 4; 

  reg[x] &= reg[y]; 
}

// Set VX to VX ^ VY
inline void op8XY3(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>= 4; 

  reg[x] ^= reg[y]; 
}

// Set VX to VX + VY, set VF if carry
inline void op8XY4(WORD op) { 
  reg[0xF] = 1; 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>= 4; 
  int sum = reg[x] + reg[y]; 
  if(sum > 255) reg[0xF] = 0; 

  reg[x] = sum; 
}

// Set VX to VX - VY, set VF if borrow
inline void op8XY5(WORD op) { 
  reg[0xF] = 1; 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>= 4; 
  int diff = reg[x] - reg[y]; 
  if(diff < 0) reg[0xF] = 0; 

  reg[x] = diff; 
}

// Shift VX right by one, set VF to least significant bit before shift
inline void op8XY6(WORD op) {
  int x = op & XMASK; 
  x >>= 8; 
  reg[0xF] = reg[x] & 0x000F; 

  reg[x] >>= 1;
}

// Set VX to VY - VX, set VF if borrow
inline void op8XY7(WORD op) { 
  reg[0xF] = 1; 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>=4; 
  int diff = reg[y] - reg[x]; 
  if(diff < 0) reg[0xF] = 0; 

  reg[x] = diff; 
}

// Shift VX left by one, set VF to most significant bit before shift
inline void op8XYE(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  reg[0xF] = reg[x] & 0xF000; 

  reg[x] <<= 1; 
}

// If VX != VY, skip next instruction
inline void op9XY0(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  int y = op & 0x00F0; 
  y >>=4; 
  if(reg[x] != reg[y]) pc += 2; 
}

// Set address I to NNN
inline void opANNN(WORD op) { 
  addrI = op & 0x0FFF; 
}

// Jump to NNN + V0
inline void opBNNN(WORD op) { 
  pc = (op & 0x0FFF) + reg[0]; 
}

// Set VX to random number & NN
inline void opCXNN(WORD op) {
  int x = op & XMASK; 
  x >>= 8; 
  int nn = op & 0x00FF; 

  reg[x] = rand() % 255;
  reg[x] &= nn; 
}

// Draw sprite to display 
inline void opDXYN(WORD op) { 
  int x = (op & XMASK)>>8; 
  int y = (op & 0x00F0)>>4; 
  int h = op & 0x000F; 
  int xcor = reg[x]; 
  int ycor = reg[y];
  reg[0xF] = 1; 

  for(int yl = 0; yl < h; ++yl) { 
    const BYTE & data = gmem[addrI + yl]; 
    int j = 7; 
    for(int i=0;i<8;i++,j--) { 
      BIT & dp = disp[xcor+j][ycor+yl]; 
      if(dp == 1) reg[0xF] = 0; 
      dp ^= data & (1<<i); 
    }
  }
}

// Skip next instruction if key in VX is pressed
inline void opEX9E(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  if(keyboard[reg[x]]) pc += 2; 
}

// Skip next instruction if key in VX isn't pressed
inline void opEXA1(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  if(!keyboard[reg[x]]) pc += 2; 
}

// Set VX to value of delay timer
inline void opFX07(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  reg[x] = d_timer; 
}

// A key press is awaited, and then stored in VX
inline void opFX0A(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  for(int i=0; i < 16; ++i) { 
    if(keyboard[i]) {
      reg[x] = keyboard[i]; 
      return; 
    }
  }
}

// Set delay timer to VX
inline void opFX15(WORD op) {
  int x = op & XMASK; 
  x >>= 8; 
  d_timer = reg[x]; 
}

// Set sound timer to VX
inline void opFX18(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  s_timer = reg[x]; 
}

// Add VX to I
inline void opFX1E(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 
  reg[0xF] = 0; 
  int sum = addrI + reg[x]; 
  if(sum > 0xFFF) reg[0xF] = 1; 
  addrI = sum; 
}

// Set address I to location of sprite for character in VX
inline void opFX29(WORD op) {
  int x = op & XMASK; 
  x >>= 8; 
  addrI = reg[x]; 
}

// Store decimal of VX 
inline void opFX33(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 

  int val = reg[x]; 
  int hunds = val/100; 
  int tens  = (val/10) % 10; 
  int ones  = val % 10; 

  gmem[addrI]   = hunds; 
  gmem[addrI+1] = tens; 
  gmem[addrI+2] = ones; 
}

inline void opFX55(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 

  for(int i=0; i <= x; ++i) { 
    gmem[addrI + i] = reg[i]; 
  }
}

inline void opFX65(WORD op) { 
  int x = op & XMASK; 
  x >>= 8; 

  for(int i=0; i <= x; ++i) { 
    reg[i] = gmem[addrI + i]; 
  }
}

inline void stepCPU()
{
    // Fetch opcode
    WORD op = getNextOp();

    // Decipher opcode and call opcode function
    switch(op & 0xF000) { 
      case 0x0000 :  
      {
        switch(op & 0x000F) { 
          case 0x0000: op00E0(op); break; 
          case 0x000E: op00EE(op); break; 
        }
      } break;
      case 0x1000 : op1NNN(op); break; 
      case 0x2000 : op2NNN(op); break; 
      case 0x3000 : op3XNN(op); break;
      case 0x4000 : op4XNN(op); break;
      case 0x5000 : op5XY0(op); break;
      case 0x6000 : op6XNN(op); break;
      case 0x7000 : op7XNN(op); break;
      case 0x8000 : 
      {
        switch(op & 0x000F) { 
          case 0x0000: op8XY0(op); break; 
          case 0x0001: op8XY1(op); break;
          case 0x0002: op8XY2(op); break;
          case 0x0003: op8XY3(op); break; 
          case 0x0004: op8XY4(op); break; 
          case 0x0005: op8XY5(op); break;
          case 0x0006: op8XY6(op); break;
          case 0x0007: op8XY7(op); break;
          case 0x000E: op8XYE(op); break;
        }
      } break; 
      case 0x9000 : op9XY0(op); break; 
      case 0xA000 : opANNN(op); break;
      case 0xB000 : opBNNN(op); break;
      case 0xC000 : opCXNN(op); break;
      case 0xD000 : opDXYN(op); break;
      case 0xE000 : 
      {
        switch(op & 0x000F) { 
          case 0x000E : opEX9E(op); break;
          case 0x0001 : opEXA1(op); break; 
        }
      } break; 
      case 0xF000 : 
      {
        switch(op & 0x000F) {
          case 0x0007 : opFX07(op); break;
          case 0x000A : opFX0A(op); break;
          case 0x0005 : 
          {
            switch(op & 0x00F0) { 
              case 0x0010 : opFX15(op); break;
              case 0x0050 : opFX55(op); break;
              case 0x0060 : opFX65(op); break;
            }
          } break; 
          case 0x0008 : opFX18(op); break;
          case 0x000E : opFX1E(op); break;
          case 0x0009 : opFX29(op); break;
          case 0x0003 : opFX33(op); break;
        }
      } break;
      default: break; 
    }
}

/* SDL functions
 */
inline void putpixel(SDL_Surface *surface, Sint16 x, Sint16 y, const Uint32 &color)
{
  Sint16 xcor  = x*10; 
  Sint16 ycor  = y*10; 

  SDL_Rect rect = {xcor,ycor,10,10}; 
  SDL_FillRect(surface,&rect,color);  
}

inline void drawScreen() { 
  static const  Uint32 white = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff); 
  static const Uint32 black = SDL_MapRGB(screen->format, 0, 0, 0); 
  if(SDL_MUSTLOCK(screen)) {
    if(SDL_LockSurface(screen) < 0) { 
      fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError()); 
      return;
    }
  }
  for(int y =0; y < 32; ++y) { 
    for(int x=0; x < 64; ++x) { 
      if(disp[x][y]) { 
        putpixel(screen,x,y,white); 
      }
      else { 
        putpixel(screen,x,y,black); 
      }
    }
  }

  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen); 
  SDL_UpdateRect(screen,0,0,0,0);  
}

int main()
{
  // Initialize SDL
  SDL_Init(SDL_INIT_VIDEO); 
  screen = SDL_SetVideoMode(640, 320, 32, SDL_SWSURFACE); 
  SDL_Event event; 
  srand( time(NULL) ); 

  SDL_Flip(screen); 
  resetCPU("PONG2"); 

  bool done = false; 
  const Uint32 FRAMERATE = 60; //run at 60hz
  Uint32 old_time; 
  Uint32 dt;

  while(!done) { 
    old_time = SDL_GetTicks(); 
    while(SDL_PollEvent(&event)) { 
      if(event.type == SDL_QUIT) done = true; 
    }

    stepCPU(); 
    if(d_timer > 0) d_timer--; 
    if(s_timer > 0) s_timer--;

    if(reg[0xF]) { 
      drawScreen();
      SDL_Flip(screen); 
    }

    dt = SDL_GetTicks() - old_time; 
    if(dt < 1000/FRAMERATE) { 
      SDL_Delay( (1000/FRAMERATE) - dt);
    }
  }

  // Cleanup SDL
  SDL_FreeSurface(screen);  
  SDL_Quit(); 
  return 0; 
}
