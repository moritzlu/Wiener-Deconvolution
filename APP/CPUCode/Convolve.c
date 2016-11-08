#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "./stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./stb_image_write.h"

#include "Maxfiles.h"
#include "MaxSLiCInterface.h"

//!!!!!!!!! keep the combined Blocksize BigBlock * SmallBlock equal to blocksize chosen in Kernel
#define BigBlockSizeX 48
#define BigBlockSizeY 48
#define SmallBlockSizeX 4
#define SmallBlockSizeY 4
#define dummy 0 //number of dummy elements after each BB


struct imaginaryNumbers
{
	float real;
	float imaginary;
};

struct imaginaryNumbersDouble
{
	double real;
	double imaginary;
};


void CPUtranspose(struct imaginaryNumbers*, struct imaginaryNumbers*, int, int);
void toBlockedStructure(unsigned char*, int*, int, int, int);
void toBlockedStructureKernel(struct imaginaryNumbersDouble* , struct imaginaryNumbersDouble* , int , int);
void toNormalStructure(float*, unsigned char*, int, int, int);
void fWriteBlocked(FILE*, struct imaginaryNumbers* ,int ,int);
double* parseCsv(int sizeX, int sizeY, int complex, char* filename);
double* parseOctave(int sizeX, int sizeY, int complex, char* filename);


int main(void)
{
	//round up to always process full BigBlocks (input image dimensions)
	int xMatrixDim= 5952;
	int xBBcount = (int)ceil(xMatrixDim / BigBlockSizeX / SmallBlockSizeX);
	int yMatrixDim= 3840;
	int yBBcount = (int)ceil(yMatrixDim / BigBlockSizeY / SmallBlockSizeY);

	int  byteperpixel = 1;
	unsigned char* data = stbi_load("./blurandNoise6464LP.bmp", &xMatrixDim, &yMatrixDim, &byteperpixel, 0);

	int size = xMatrixDim * yMatrixDim;
	int sizeBytes = size * sizeof(int);
	int *input = malloc(sizeBytes);
	if(input == NULL)
	{
		printf("failed to allocate memory\n");
		return -1;
	}
	struct imaginaryNumbersDouble *kernel = malloc(sizeof(double) *256 *256 * 2);
	float *output = calloc(sizeBytes * 2, 1);

	// parse CSV matrix file
	double* kernelBuf = parseCsv(256,256,1,"./WienerFilter.txt");
	if(kernelBuf == NULL)
	{
		printf("failed to allocate memory\n");
		return -1;
	}
	
	toBlockedStructureKernel((struct imaginaryNumbersDouble*)kernelBuf,
			kernel, SmallBlockSizeX, SmallBlockSizeY);
	free(kernelBuf);
	toBlockedStructure(data, input, xMatrixDim, xBBcount, yBBcount);
	free(data);

	max_file_t *maxfile = Convolve_init();
	max_engine_t *engine = max_load(maxfile, "*");
	printf("writing to memory\n");
	Convolve_writeLMem_actions_t writeaction = {0, sizeBytes, input};
	Convolve_writeLMem_run(engine,&writeaction);

	printf("Running on DFE \n");

	max_actions_t* actions = max_actions_init(maxfile,"default");
	Convolve_actions_t run_scalar;
	run_scalar.param_BlockNumber = xBBcount * yBBcount;
	run_scalar.instream_Kernel = kernel;

	Convolve_run(engine, &run_scalar);
	printf("reading from memory\n");
	Convolve_readLMem_actions_t readaction = {0, sizeBytes , output};
	Convolve_readLMem_run(engine, &readaction);

	max_unload(engine);
	free(kernel);


	unsigned char* imageBuffer = malloc(xMatrixDim * yMatrixDim * sizeof(char) * 3);
	if(imageBuffer == NULL)
	{
		printf("failed to allocate memory\n");
		return -1;
	}
	toNormalStructure(output, imageBuffer, xMatrixDim, xBBcount, yBBcount);
	free(output);

	stbi_write_bmp("./result.bmp", xMatrixDim,y MatrixDim, byteperpixel, imageBuffer);
	free(imageBuffer);

	printf("Done.\n");
	return 0;
}



void toBlockedStructure(unsigned char* in, int* out, int xMatrixDim, int xBigBlockcount, int yBigBlockcount)
{
	int address,offset,check;
	for(int y = 0; y < yBigBlockcount; y++)
	{
		for(int x = 0; x < xBigBlockcount; x++)
		{
			offset = (x + y * xBigBlockcount) * (BigBlockSizeX * BigBlockSizeY * SmallBlockSizeX * SmallBlockSizeY + dummy);
			for(int mediumy = 0; mediumy < BigBlockSizeY; mediumy++) // one Bigblock 16*4 x 16*4
			{
				for(int mediumx = 0; mediumx < BigBlockSizeX; mediumx++)
				{
					for(int smally = 0; smally < SmallBlockSizeY; smally++)
					{
						for(int smallx = 0; smallx < SmallBlockSizeX; smallx++)
						{
							///check == addresses to read from a linear array to get the blocked representation
							 check = smallx + xMatrixDim * smally
									 + mediumy * xMatrixDim * SmallBlockSizeY + mediumx * SmallBlockSizeX
									 + SmallBlockSizeX * BigBlockSizeX * (x + y * SmallBlockSizeY * BigBlockSizeY * xBigBlockcount);
							address = ((mediumy * BigBlockSizeX + mediumx) * SmallBlockSizeX * SmallBlockSizeY + SmallBlockSizeX * smally + smallx);
							///FIXME *3 due to image read library, change for other libs
							out[offset + address] = in[check * 3];
						}
					}
				}
			}
		}
	}
}
void toBlockedStructureKernel(struct imaginaryNumbersDouble* in, struct imaginaryNumbersDouble* out, int sBlockSizeX, int sBlockSizeY)
{
	int address, check;
	struct imaginaryNumbersDouble buffer;
	for(int mediumy = 0; mediumy<256/sBlockSizeY; mediumy++)
	{
		for(int mediumx = 0; mediumx<256/sBlockSizeX; mediumx++)
		{
			for(int smally = 0; smally<sBlockSizeY; smally++)
			{
				for(int smallx = 0; smallx<sBlockSizeX; smallx++)
				{
					 check = smallx + 256 * smally + mediumy * 256 * sBlockSizeY + mediumx * sBlockSizeX;
					 address = ((mediumy * 256/sBlockSizeX + mediumx) * sBlockSizeX * sBlockSizeY + sBlockSizeX * smally + smallx);
					 buffer = in[check];
					 out[address] = buffer;
				}
			}
		}
	}
}
void toNormalStructure(float* in, unsigned char* out, int xMatrixDim, int xBigBlockcount, int yBigBlockcount)
{
	int address, offset, check;
	unsigned char buffer;
	for(int y = 0; y < yBigBlockcount; y++)
	{
		for(int x = 0; x < xBigBlockcount; x++)
		{
			offset = (x + y * xBigBlockcount) * (BigBlockSizeX * BigBlockSizeY * SmallBlockSizeX * SmallBlockSizeY + dummy);
			for(int mediumy = 0; mediumy < BigBlockSizeY; mediumy++)
			{
				for(int mediumx = 0; mediumx < BigBlockSizeX; mediumx++)
				{
					for(int smally = 0; smally < SmallBlockSizeY; smally++)
					{
						for(int smallx = 0; smallx < SmallBlockSizeX; smallx++)
						{
							///check == addresses to read from a linear array to get the blocked representation
							 check = smallx + xMatrixDim * smally
									 + mediumy * xMatrixDim  * SmallBlockSizeY + mediumx * SmallBlockSizeX
									 + SmallBlockSizeX * BigBlockSizeX * (x + y * SmallBlockSizeY * BigBlockSizeY * xBigBlockcount);
							address = ((mediumy * BigBlockSizeX + mediumx) * SmallBlockSizeX * SmallBlockSizeY + SmallBlockSizeX*smally + smallx);
							buffer = (unsigned char) in[offset+address];
                                                        ///FIXME *3 due to image read library, change for other libs
							out[check*3] = buffer;
							out[check*3+1] = buffer;
							out[check*3+2] = buffer;
						}
					}
				}
			}
		}
	}
}
 double* parseOctave(int sizeX, int sizeY, int complex, char* filename)
{
	 FILE * pFile = fopen(filename, "r");
	 // obtain file size
	 fseek (pFile, 0, SEEK_END);
	 long lSize = ftell(pFile);
	 rewind (pFile);
	 char * data = malloc(lSize * sizeof(char));
	 if(data == NULL)
		 {printf("failed to allocate data for input buffer\n"); return NULL;}
	 printf("data read %zu\n",fread(data,sizeof(char),lSize,pFile));

	 int size = sizeX * sizeY;
	 if(complex)
		 size = size * 2;

	 double * out = (double *) malloc(size * sizeof(double));
	 if(out == NULL)
		 {printf("failed to allocate data for output buffer\n"); return NULL;}
	 char * buf = strchr(data, '(');
	 if(buf == NULL)
		 {printf("no ( found, input file invalid\n"); return NULL;}

	 buf = strtok(buf, " (,)");
	 out[0] = atof(buf);
	 int i = 1;
	 for(; (buf = strtok(NULL, " (,)\n")); i++)
	 {
            out[i] = atof(buf);
            if(i > size - 1)
            {
                printf("Input data bigger than the intended array size\n");
                break;
            }
	 }
	 printf("elements parsed %d\n", i);
	 free(data);
	 fclose(pFile);
	 return out;
}

double* parseCSV( int sizeX, int sizeY, int complex, char* filename)
{
   FILE * pFile = fopen(filename, "r");
   fseek (pFile, 0, SEEK_END);
   long lSize = ftell (pFile);
   rewind (pFile);
   char * data = malloc(lSize * sizeof(char));
    
   if(data ==NULL)
        {printf("failed to allocate data for input buffer\n"); return -1;}
   printf("data read %zu\n",fread(data,sizeof(char),lSize,pFile));

   int size = sizeX * sizeY;
   if(complex)
       size = size * 2;
 
   double * out = (double *) malloc(size * sizeof(double));
   if(out == NULL)
        {printf("failed to allocate data for output buffer\n"); return -1;}

   int i = 0, endmarker = 0;
   char * buf;
   char * prevpos = data;
   char * pos = strchr(data, ',');
   if(pos == NULL) {printf("No commas found, file error\n"); return -1;}
   int csvSize = pos - prevpos;
   char * tokenBuf = malloc(csvSize + 1);
   memset(tokenBuf, 0, csvSize + 1);
   memcpy(tokenBuf, prevpos, csvSize);
   
   while(1)
   {
        buf = strtok(tokenBuf, " +-i"); // get real part
        out[i] = atof(buf);
        i++;
        if(complex)
        {
            buf = strtok(NULL, " +-i"); //get imaginary part
            if(buf == NULL) // check if imaginary part is existent
                out[i] = 0.0;
            else
                out[i] = atof(buf); // convert text to double
            i++;
        }
                
        if(i > size - 1)
        {
            printf("Input data bigger than the intended array size\n");
            break;
        }
        if(endmarker == 1) // EOF reached 
            break;
        prevpos = pos + 1; // move pointer to the beginning of the next token
        pos = strchr(pos + 1, ',') ; //serach for next "," delimiter

        if(pos == NULL) // last element in the list, use strcpy to automatically stop at the end of string
        {
            endmarker = 1;
            int length = strlen(prevpos);
            tokenBuf = malloc(length + 1); //zero termination is not included
            memset(tokenBuf, 0, length + 1);
            strcpy(tokenBuf, prevpos);
            
        }
        else
        {
            //extract the tokens in between the delimiters 
            if(pos - prevpos > csvSize) 
                tokenBuf = malloc(pos - prevpos + 1);
            if(tokenBuf== NULL) {printf("could not allocate memory\n"); return -1;}
            csvSize = pos - prevpos;
            memset(tokenBuf, 0, csvSize + 1);
            memcpy(tokenBuf, prevpos, csvSize); // prepare string for strtok
        }
   }
   free(tokenBuf);
   printf("elements parsed %d\n", i);
   free(data);
   fclose(pFile);
   return out;
}
