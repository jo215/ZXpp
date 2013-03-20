#include "ula.h"

//	This class represent the Ferranti ULA chip

//	Constructor
ULA::ULA(ID3D11Device* device, ID3D11DeviceContext* dcontext, HINSTANCE hinstance, HWND hwnd, ID3D11RenderTargetView* backBuffer)
{
	backbufferview = backBuffer;
	context = dcontext;
	cache = new TextureCache(device, dcontext);
	spriteBatch.reset( new SpriteBatch( dcontext) );
	screenScale = 2.0f;
	framesGenerated = 0;
	micActive = false;
	earActive = false;
	update = true;
	border = 0;

	memory = new Memory(nullptr);
	
	z80 = new Z80(memory);
	z80->AddDevice(this);
	
	speaker = new Loudspeaker();
	speaker->Init(hwnd);

	//	Init direct input for keyboard
	DirectInput8Create(hinstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, NULL);
	directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	keyboard->SetDataFormat(&c_dfDIKeyboard);
	keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
	keyboard->Acquire();

	//	Setup the available colors
	XMVECTORF32 c0 =  {0.f,0.f,0.f,1.f};
	XMVECTORF32 c1 =  {0.f,0.f,0.8f,1.f};
	XMVECTORF32 c2 =  {0.8f,0.f,0.f,1.f};
	XMVECTORF32 c3 =  {0.8f,0.f,0.8f,1.f};
	XMVECTORF32 c4 =  {0.f,0.8f,0.f,1.f};
	XMVECTORF32 c5 =  {0.f,0.8f,0.8f,1.f};
	XMVECTORF32 c6 =  {0.8f,0.8f,0.f,1.f};
	XMVECTORF32 c7 =  {0.8f,0.8f,0.8f,1.f};
	XMVECTORF32 c8 =  {0.f,0.f,0.f,1.f};
	XMVECTORF32 c9 =  {0.f,0.f,1.f,1.f};
	XMVECTORF32 c10 =  {1.f,0.f,0.f,1.f};
	XMVECTORF32 c11 =  {1.f,0.f,1.f,1.f};
	XMVECTORF32 c12 =  {0.f,1.f,0.f,1.f};
	XMVECTORF32 c13 =  {0.f,1.f,1.f,1.f};
	XMVECTORF32 c14 =  {1.f,1.f,0.f,1.f};
	XMVECTORF32 c15 =  {1.f,1.f,1.f,1.f};

	colors[0] = c0;
	colors[1] = c1;
	colors[2] = c2;
	colors[3] = c3;
	colors[4] = c4;
	colors[5] = c5;
	colors[6] = c6;
	colors[7] = c7;
	colors[8] = c8;
	colors[9] = c9;
	colors[10] = c10;
	colors[11] = c11;
	colors[12] = c12;
	colors[13] = c13;
	colors[14] = c14;
	colors[15] = c15;

	//LoadSNA("hobbit.sna");
}

//	Destructor
ULA::~ULA()
{
	if (cache) cache->Cleanup();
	
	if (keyboard)
	{
		keyboard->Unacquire();
		keyboard->Release();
		keyboard = 0;
	}

	if (directInput)
	{
		directInput->Release();
		directInput = 0;
	}
}

//	Runs the z80 for 69888 Tstates (1/50 second) and generates 50Hz interrupt
void ULA::updateFrame()
{
	//	Once audio is implemented, we will sync to it here
	//if (speaker.sfx.PendingBufferCount > 1)
	//       update = false;
	//   else
	//       update = true;

	//	For now, caller must ensure calling this method at the correct rate 
	if (update)
	{
		//  Generate the 50Hz clock interrupt
		z80->Interrupt(false);

		//  Run the cpu for a frame's worth of T-states (69888)

        //  for 44.1KHz audio / 50 frames we require 882 audio samples per frame
        for (int i = 0; i < 882; i++)
        {
            //  69888 / 882 = 79 TStates per sample
            z80->Run(false, 79);
            //  we just take the last value sent to the ear output
            if (earActive)
            {
				buffer.push_back(255);
				buffer.push_back(255);
            }
            else
            {
                buffer.push_back(0);
				buffer.push_back(0);
            }
        }
		speaker->Play(buffer);
		buffer.clear();

        //  Check control keys
        getUserInput();

        framesGenerated++;
	}
	
}

//	Draws the screen
void ULA::render()
{
	
	//  Pixels in RAM 16384-22527
    //  Each 1/3 of screen (64 lines / 8 characters) stored in alternate lines of 256 pixels 
            
    //DrawBorder();
    //  Cheat-draw the border by clearing in the correct color - this wont work for the flashing load border
	context->ClearRenderTargetView( backbufferview, colors[border]);
	
	spriteBatch->Begin(SpriteSortMode_Deferred);
    //  Main screen display
    for (int line = 0; line < 24; line++)
    {
        for (int cha = 0; cha < 32; cha++)
        {
            //  Attribute (color) map stored from 22528
			int attribute = memory->Read(22528 + 32 * line + cha, true);
            //  Bits 0-2 set ink color
            int ink = attribute & 7;
            //  Bits 3-5 set paper (background) color
            int paper = (attribute >> 3) & 7;
            if ((attribute & 64) == 64)
            {
                //  Bit 6 sets bright variant;
                ink += 8;
                paper += 8;
            }

            //  Starting x-position for each character
            float startX = (cha * 8 + borderWidth) * screenScale;

            //  Draw an 8x8 block in current paper color for each character position
			spriteBatch->Draw(cache->getBlock(), XMFLOAT2(startX, (borderHeight + (line * 8)) * screenScale), NULL, colors[paper], 0.f, XMFLOAT2(0, 0), screenScale);

            //  Draw each pixel line
            for (int row = 0; row < 8; row++)
            {
				int pixels = memory->Read(16384 + 2048 * (line / 8) + 32 * (line - 8 * (line / 8)) + 256 * row + cha, true);
                //  Bit 7 sets flashing - invert pixels
                if ((attribute & 128) == 128 && framesGenerated % 32 <= 15)
                {
                    pixels = 255 - pixels;
                }

                float y = (line * 8 + row + borderHeight) * screenScale;

				spriteBatch->Draw(cache->getTexture(pixels), XMFLOAT2(startX, y), NULL, colors[ink], 0.f, XMFLOAT2(0,0), screenScale);                   
            }
        }
    }

	spriteBatch->End();
}

void ULA::getUserInput()
{

	ZeroMemory(keyboardState, 256);
	HRESULT hr = keyboard->GetDeviceState( sizeof(keyboardState), keyboardState );

	if (FAILED(hr))
	{
		   // If input is lost then acquire it
		   hr = keyboard->Acquire();
		   if( hr == DIERR_INPUTLOST ) 
		   {          
				 hr = keyboard->Acquire();
		   }
		   // Return if failed
		   if (FAILED(hr))
			 return;
		   
		   keyboard->GetDeviceState( sizeof(keyboardState), keyboardState );
	}
	
	//	Immediately quit application
	if (keyboardState[DIK_ESCAPE] & 0x80)
	{
		exit(0);
	}

	//	Save snapshot
	if (keyboardState[DIK_F5] & 0x80)
	{
		//	#TODO
	}
}

int ULA::Read(int port)
{
	//  Port 0xFE should be used to address the ULA, but it actually responds to every even port
        if (port % 2 == 0)
        {
            //  0 on a high-byte bit selects a half row of keys
            int high = (port >> 8);
            //  0 in 5 lowest bits of result means corresponding key pressed
            int finalKeys = 31;
            //  Result (of 5 lowest bits) is AND of all selected half-rows
            int keyLine = 31;
            if ((high & 1) == 0)
            {
				if (keyboardState[DIK_LSHIFT] & 0x80) keyLine -= 1;
                if (keyboardState[DIK_Z] & 0x80) keyLine -= 2;
                if (keyboardState[DIK_X] & 0x80) keyLine -= 4;
                if (keyboardState[DIK_C] & 0x80) keyLine -= 8;
                if (keyboardState[DIK_V] & 0x80) keyLine -= 16;
                finalKeys &= keyLine;
            }
            if ((high & 2) == 0)
            {
                if (keyboardState[DIK_A] & 0x80) keyLine -= 1;
                if (keyboardState[DIK_S] & 0x80) keyLine -= 2;
                if (keyboardState[DIK_D] & 0x80) keyLine -= 4;
                if (keyboardState[DIK_F] & 0x80) keyLine -= 8;
                if (keyboardState[DIK_G] & 0x80) keyLine -= 16;
                finalKeys &= keyLine;
            }
            if ((high & 4) == 0)
            {
                if (keyboardState[DIK_Q] & 0x80) keyLine -= 1;
                if (keyboardState[DIK_W] & 0x80) keyLine -= 2;
                if (keyboardState[DIK_E] & 0x80) keyLine -= 4;
                if (keyboardState[DIK_R] & 0x80) keyLine -= 8;
                if (keyboardState[DIK_T] & 0x80) keyLine -= 16;
                finalKeys &= keyLine;
            }
            if ((high & 8) == 0)
            {
                if (keyboardState[DIK_1] & 0x80) keyLine -= 1;
                if (keyboardState[DIK_2] & 0x80) keyLine -= 2;
                if (keyboardState[DIK_3] & 0x80) keyLine -= 4;
                if (keyboardState[DIK_4] & 0x80) keyLine -= 8;
                if (keyboardState[DIK_5] & 0x80) keyLine -= 16;
                finalKeys &= keyLine;
            }
            if ((high & 16) == 0)
            {
                if (keyboardState[DIK_0] & 0x80) keyLine -= 1;
                if (keyboardState[DIK_9] & 0x80) keyLine -= 2;
                if (keyboardState[DIK_8] & 0x80) keyLine -= 4;
                if (keyboardState[DIK_7] & 0x80) keyLine -= 8;
                if (keyboardState[DIK_6] & 0x80) keyLine -= 16;
                finalKeys &= keyLine;
            }
            if ((high & 32) == 0)
            {
                if (keyboardState[DIK_P] & 0x80) keyLine -= 1;
                if (keyboardState[DIK_O] & 0x80) keyLine -= 2;
                if (keyboardState[DIK_I] & 0x80) keyLine -= 4;
                if (keyboardState[DIK_U] & 0x80) keyLine -= 8;
                if (keyboardState[DIK_Y] & 0x80) keyLine -= 16;
                finalKeys &= keyLine;
            }
            if ((high & 64) == 0)
            {
				if (keyboardState[DIK_RETURN] & 0x80) keyLine -= 1;
                if (keyboardState[DIK_L] & 0x80) keyLine -= 2;
                if (keyboardState[DIK_K] & 0x80) keyLine -= 4;
                if (keyboardState[DIK_J] & 0x80) keyLine -= 8;
                if (keyboardState[DIK_H] & 0x80) keyLine -= 16;
                finalKeys &= keyLine;
            }
            if ((high & 128) == 0)
            {
                if (keyboardState[DIK_SPACE] & 0x80) keyLine -= 1;
                if (keyboardState[DIK_RSHIFT] & 0x80) keyLine -= 2;
                if (keyboardState[DIK_M] & 0x80) keyLine -= 4;
                if (keyboardState[DIK_N] & 0x80) keyLine -= 8;
                if (keyboardState[DIK_B] & 0x80) keyLine -= 16;
                finalKeys &= keyLine;
            }
            //  Bit 6 is the EAR input port
            //  Leave to 1 for now as no tape input
            //  Bits 5 and 7 always 1
            finalKeys |= 224;
            return finalKeys;
        }
        else if ((port & 0xff) == 0x1f)
        {
            //  Kempston Joystick emulation
            //  Allow for cursor keys to simulate joystick control

            int joystickState = 0;
            if (keyboardState[DIK_UP] & 0x80)
            {
                joystickState = joystickState | 8;
            }
            if (keyboardState[DIK_DOWN] & 0x80)
            {
                joystickState = joystickState | 4;
            }
            if (keyboardState[DIK_LEFT] & 0x80)
            {
                joystickState = joystickState | 2;
            }
            if (keyboardState[DIK_RIGHT] & 0x80)
            {
                joystickState = joystickState | 1;
            }
			if (keyboardState[DIK_LCONTROL] & 0x80)
            {
                joystickState = joystickState | 16;
            }
            return joystickState;
        }
        else
        {
            //  Default value to return if port not recognised
            return 0;
        }
}

void ULA::Write(int port, int dataByte)
{
	//  Port 0xFE should be used to address the ULA, but it actually responds to every even port
    if (port % 2 == 0)
    {
        //  Bits 0 - 2 change the border color
        border = dataByte & 7;
        //  0 in bit 3 activates MIC
        micActive = (dataByte & 8) == 8 ? false : true;
        //  1 in bit 4 activates EAR
        earActive = (dataByte & 16) == 16 ? true : false;
        //  Apparently if one gets activated they both get activated...
        //  need to check this behaviour
    }
}

void ULA::LoadSNA(string fileName)
{
	ifstream myfile (fileName, ios::binary);
	if (myfile.is_open())
	{
		myfile.seekg(0, ios::end);
		size_t fileSize = myfile.tellg();
		myfile.seekg(0, ios::beg);
		vector<byte> data(fileSize, 0);
		myfile.read(reinterpret_cast<char*>(&data[0]), fileSize);
		myfile.close();

		z80->I = data[0];
		z80->L2 = data[1];
		z80->H2 = data[2];
		z80->E2 = data[3];
		z80->D2 = data[4];
		z80->C2 = data[5];
		z80->B2 = data[6];
		z80->F2 = data[7];
		z80->A2 = data[8];
		z80->L = data[9];
		z80->H = data[10];
		z80->E = data[11];
		z80->D = data[12];
		z80->C = data[13];
		z80->B = data[14];
		z80->IYL = data[15];
		z80->IYH = data[16];
		z80->IXL = data[17];
		z80->IXH = data[18];
		if ((data[19] & 4) == 4)
		{
			z80->IFF2 = true;
		}
		else
		{
			z80->IFF2 = false;
		}
		z80->R = data[20];
		z80->F = data[21];
		z80->A = data[22];
		z80->SP = data[23];
		z80->SP += (data[24] << 8);
		z80->interruptMode = data[25];
		border = data[26];
		for (int i = 0; i < 49152; i++)
		{
			memory->Write(i + 16384, data[i+27], true);
		}
		z80->RETN();
	} else {
		cout << "Couldn't open ROM file." << endl;
	}
}