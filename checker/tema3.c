#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_PIXEL_VALUE 255
#define MIN_PIXEL_VALUE 0
#define ERROR_CODE -1

/* Tells the compiler not to add padding for these structs. This may
   be useful when reading/writing to binary files.
   http://stackoverflow.com/questions/3318410/pragma-pack-effect
*/
#pragma pack(1)

typedef struct 
{
    unsigned char  fileMarker1; /* 'B' */
    unsigned char  fileMarker2; /* 'M' */
    unsigned int   bfSize; /* File's size */
    unsigned short unused1; /* Aplication specific */
    unsigned short unused2; /* Aplication specific */
    unsigned int   imageDataOffset; /* Offset to the start of image data */
} bmp_fileheader;

typedef struct 
{
    unsigned int   biSize; /* Size of the info header - 40 bytes */
    signed int     width; /* Width of the image */
    signed int     height; /* Height of the image */
    unsigned short planes;
    //Number of bits per pixel = 3 * 8 (for each channel R, G, B we need 8 bits
    unsigned short bitPix;  
    unsigned int   biCompression; /* Type of compression */
    unsigned int   biSizeImage; /* Size of the image data */
    int            biXPelsPerMeter;
    int            biYPelsPerMeter;
    unsigned int   biClrUsed;
    unsigned int   biClrImportant;
} bmp_infoheader;

typedef struct 
{
    unsigned char Green;
    unsigned char Red;
    unsigned char Blue;
} bmp_image;

//pentru a nu da stackoverflow la algoritmul de umplere facem un singur pointer
typedef struct
{
    bmp_fileheader *fileHeader;
    bmp_infoheader *infoHeader;
    bmp_image **physicalImage;
    int **matrixFrequence;
    int numberZone, treshold;
    bmp_image pixelReference;
} flood_fill;

#pragma pack()

//functie de citire a fileheaderului si a infoheaderului
void read_data(FILE *image, bmp_fileheader *fileHeader,
    bmp_infoheader *infoHeader){

    fread(&fileHeader->fileMarker1, sizeof(unsigned char), 1, image);
    fread(&fileHeader->fileMarker2, sizeof(unsigned char), 1, image);
    fread(&fileHeader->bfSize, sizeof(unsigned int), 1, image);
    fread(&fileHeader->unused1, sizeof(unsigned short), 1, image);
    fread(&fileHeader->unused2, sizeof(unsigned short), 1, image);
    fread(&fileHeader->imageDataOffset, sizeof(unsigned int), 1, image);

    fread(&infoHeader->biSize, sizeof(unsigned int), 1, image);
    fread(&infoHeader->width, sizeof(signed int), 1, image);
    fread(&infoHeader->height, sizeof(signed int), 1, image);
    fread(&infoHeader->planes, sizeof(unsigned short), 1, image);
    fread(&infoHeader->bitPix, sizeof(unsigned short), 1, image);
    fread(&infoHeader->biCompression, sizeof(unsigned int), 1, image);
    fread(&infoHeader->biSizeImage, sizeof(unsigned int), 1, image);
    fread(&infoHeader->biXPelsPerMeter, sizeof(int), 1, image);
    fread(&infoHeader->biYPelsPerMeter, sizeof(int), 1, image);
    fread(&infoHeader->biClrUsed, sizeof(unsigned int), 1, image);
    fread(&infoHeader->biClrImportant, sizeof(unsigned int), 1, image);
}

//functie de citire a pozei pe pixeli(codul rgb)
void read_physicalImage(bmp_image **physicalImage, FILE *image,
    bmp_infoheader *infoHeader,  bmp_fileheader * fileHeader){

    fseek(image, fileHeader -> imageDataOffset, SEEK_SET);
    for(int i = 0; i < infoHeader -> height; i++){
        for(int j = 0; j < infoHeader -> width; j++){
            fread(&physicalImage[infoHeader->height - i - 1][j].Blue,
                sizeof(unsigned char), 1, image);
            fread(&physicalImage[infoHeader->height - i - 1][j].Green,
                sizeof(unsigned char), 1, image);
            fread(&physicalImage[infoHeader->height - i - 1][j].Red,
                sizeof(unsigned char), 1, image);
		}
        if(infoHeader->width % 4 != 0)
        fseek(image, infoHeader->width % 4, SEEK_CUR);
    }
}

//functie caree creeaza efectiv poza nou generata(depinde cum e folosita)
//in principiu, formeaza DOAR fileheader si infoheader
void print(FILE *out, bmp_fileheader * fileHeader, bmp_infoheader *infoHeader){
    
    fwrite(&fileHeader->fileMarker1, sizeof(unsigned char), 1, out);
    fwrite(&fileHeader->fileMarker2, sizeof(unsigned char), 1, out);
    fwrite(&fileHeader->bfSize, sizeof(unsigned int), 1, out);
    fwrite(&fileHeader->unused1, sizeof(unsigned short), 1, out);
    fwrite(&fileHeader->unused2, sizeof(unsigned short), 1, out);
    fwrite(&fileHeader->imageDataOffset, sizeof(unsigned int), 1,out);

    fwrite(&infoHeader->biSize, sizeof(unsigned int), 1, out);
    fwrite(&infoHeader->width, sizeof(signed int), 1, out);
    fwrite(&infoHeader->height, sizeof(signed int), 1, out);
    fwrite(&infoHeader->planes, sizeof(unsigned short), 1, out);
    fwrite(&infoHeader->bitPix, sizeof(unsigned short), 1, out);
    fwrite(&infoHeader->biCompression, sizeof(unsigned int), 1, out);
    fwrite(&infoHeader->biSizeImage, sizeof(unsigned int), 1, out);
    fwrite(&infoHeader->biXPelsPerMeter, sizeof(int), 1, out);
    fwrite(&infoHeader->biYPelsPerMeter, sizeof(int), 1, out);
    fwrite(&infoHeader->biClrUsed, sizeof(unsigned int), 1, out);
    fwrite(&infoHeader->biClrImportant, sizeof(unsigned int), 1, out);
}

//completeaza restul pozei cu noii pixeli necesari, luand in calcul si paddingul
//corespunzator
void print_physicalImage(bmp_image **physicalImage, FILE *out,
    bmp_fileheader *fileHeader, bmp_infoheader *infoHeader){
    
    for(int i = infoHeader->height - 1; i >= 0; i--){
        for(int j = 0; j < infoHeader->width; j++){
            fwrite(&physicalImage[i][j].Blue, sizeof(unsigned char), 1, out);
            fwrite(&physicalImage[i][j].Green, sizeof(unsigned char), 1, out);
            fwrite(&physicalImage[i][j].Red, sizeof(unsigned char), 1, out);
        }
        int padding = 0;
        fwrite(&padding, sizeof(unsigned char), infoHeader->width % 4, out);
    }
}

//functie care face poza alb-negru, facand media aritmetica a codului rbg al
//unui pixel
void blackAndWhite(bmp_image **physicalImage, bmp_fileheader *fileHeader,
    bmp_infoheader *infoHeader, char image[70]){
	
    for(int i = 0; i < infoHeader->height; i++){
        for(int j = 0; j < infoHeader->width; j++){
            int noColor;
            noColor = physicalImage[i][j].Blue + physicalImage[i][j].Green +
                physicalImage[i][j].Red;
            noColor = noColor / 3;
            physicalImage[i][j].Blue = noColor;
            physicalImage[i][j].Green = noColor;
            physicalImage[i][j].Red = noColor;
        }
    }
    //formarea pozei cu numele cerut
    image[strlen(image) - 4] = '\0';
    strcat(image,"_black_white.bmp");
    FILE *out = fopen(image, "wb");
    print(out, fileHeader, infoHeader);
    print_physicalImage(physicalImage, out, fileHeader, infoHeader);
    fclose(out);
}

//primul caz al taskului, cand inaltimea este mai mica decat latimea(caz banal)
void noCropFirstCase(int white_lines, int padding, bmp_image **physicalImage,
    bmp_infoheader *infoHeader, int white_pixel, FILE *out,
    bmp_fileheader *fileHeader){

    if(white_lines % 2 == 0){
        for(int i = 0; i < white_lines / 2; i++){
            for(int j = 0; j < infoHeader->width; j++){
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
            }
            int padding = 0;
            fwrite(&padding, sizeof(unsigned char), infoHeader->width % 4, out);
        }
        print_physicalImage(physicalImage, out, fileHeader, infoHeader);

        for(int i = 0; i < white_lines / 2; i++){
            for(int j = 0; j < infoHeader->width; j++){
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
            }
            int padding = 0;
            fwrite(&padding, sizeof(unsigned char), infoHeader->width % 4, out);
        }
    }
    else{
        for(int i = 0; i < white_lines / 2 + 1; i++){
            for(int j = 0; j < infoHeader->width; j++){
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
            }
            int padding = 0;
            fwrite(&padding, sizeof(unsigned char), infoHeader->width % 4, out);
        }

        print_physicalImage(physicalImage, out, fileHeader, infoHeader);
        for(int i = 0; i < white_lines / 2; i++){
            for(int j = 0; j < infoHeader->width; j++){
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
            }
            int padding = 0;
            fwrite(&padding, sizeof(unsigned char), infoHeader->width % 4, out);
        }
    }
}

//cel de-al doilea caz al taskului 2, si anume cand inaltimea este mai mare
//decat latimea, caz putin mai special
void noCropSecondCase(int white_columns, int padding, bmp_image **physicalImage,
    bmp_infoheader *infoHeader, int white_pixel, FILE *out,
    bmp_fileheader *fileHeader){

//in functie de paritatea latimii, poza va avea un padding diferit
//CAZ I: numar de linii par
    if(white_columns % 2 == 0){
        for(int i = 0; i < infoHeader->height; i++){
            for(int j = 0; j < white_columns / 2; j++){
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
            }
            for(int j = 0; j < infoHeader->width; j++){
                fwrite(&physicalImage[infoHeader->height - i - 1][j].Blue,
                    sizeof(unsigned char), 1, out);
                fwrite(&physicalImage[infoHeader->height - i - 1][j].Green,
                    sizeof(unsigned char), 1, out);
                fwrite(&physicalImage[infoHeader->height - i - 1][j].Red,
                    sizeof(unsigned char), 1, out);
            }
            for(int j = 0; j < white_columns / 2; j++){
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
            }
            int padding = 0;
            fwrite(&padding, sizeof(unsigned char), infoHeader->width % 4, out);  
        }
    }
//cel de-al doilea caz, numar de linii impar
    else{
        for(int i = 0; i < infoHeader->height; i++){
            for(int j = 0; j < white_columns / 2; j++){
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
            }
            for(int j = 0; j < infoHeader->width; j++){
                fwrite(&physicalImage[infoHeader->height - i - 1][j].Blue,
                    sizeof(unsigned char), 1, out);
                fwrite(&physicalImage[infoHeader->height - i - 1][j].Green,
                    sizeof(unsigned char), 1, out);
                fwrite(&physicalImage[infoHeader->height - i - 1][j].Red,
                    sizeof(unsigned char), 1, out);
            }
            for(int j = 0; j < white_columns / 2 + 1; j++){
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
                fwrite(&white_pixel, sizeof(unsigned char), 1, out);
            }
            int padding = 0;
            fwrite(&padding, sizeof(unsigned char), infoHeader->width % 4, out);  
        }
    }
}
//functia care, in functie de cele doua cazuri ale taskului 2, formeaza poza
//dorita
void noCrop(bmp_image **physicalImage, bmp_fileheader *fileHeader,
    bmp_infoheader *infoHeader, char image[70]){

    image[strlen(image) - 4] = '\0';
    strcat(image,"_nocrop.bmp");
    FILE *out = fopen(image, "wb");
    unsigned char white_pixel = MAX_PIXEL_VALUE;
//latimea mai mare decat inaltimea, atunci
    if(infoHeader->width > infoHeader->height){
        int copy_height = infoHeader->height;
        infoHeader->height = infoHeader->width;
        print(out, fileHeader, infoHeader);
        infoHeader->height = copy_height;
        int white_lines, padding = 0;	
        white_lines = infoHeader->width - infoHeader->height;

        noCropFirstCase(white_lines, padding, physicalImage, infoHeader,
            white_pixel, out, fileHeader);
    }
//latimea mai mica decat inaltimea, atunci
    else if(infoHeader->width < infoHeader->height){
        int copy_width = infoHeader->width;
        infoHeader->width = infoHeader->height;
        print(out, fileHeader, infoHeader);
        infoHeader->width = copy_width;
        int white_columns, padding = 0;
        white_columns = infoHeader->height - infoHeader->width;
        noCropSecondCase(white_columns, padding, physicalImage, infoHeader,
            white_pixel, out, fileHeader);
    }
//latimea EGALA cu inaltimea, atunci
    else{
        print(out, fileHeader, infoHeader);
        print_physicalImage(physicalImage, out, fileHeader, infoHeader);
    }
    fclose(out);
}

//functie care formeaza noul pixel dorit, in functie de filtrul primit
void newConvolutionalPixels(bmp_image **physicalImage, bmp_image **rgbNewPixel,
    int i, int j, int n, int **filterMatrix, int *newRed, int *newGreen,
    int *newBlue, bmp_infoheader *infoHeader){

    int coordinatesX, coordinatesY, copy_coordinatesY;
    coordinatesX = i - n / 2;
    coordinatesY = j - n / 2;
    copy_coordinatesY = coordinatesY;

    for(int k = 0; k < n; k++){
        coordinatesY = copy_coordinatesY;
        for(int l = 0; l < n; l++){
            if(coordinatesX < MIN_PIXEL_VALUE || coordinatesY < MIN_PIXEL_VALUE
                || coordinatesX >= infoHeader->height || coordinatesY >=
                infoHeader->width){
                
                rgbNewPixel[k][l].Red = MIN_PIXEL_VALUE;
                rgbNewPixel[k][l].Green = MIN_PIXEL_VALUE;
                rgbNewPixel[k][l].Blue = MIN_PIXEL_VALUE;
            }
            else{
                rgbNewPixel[k][l].Red =
                    physicalImage[coordinatesX][coordinatesY].Red;
                rgbNewPixel[k][l].Green =
                    physicalImage[coordinatesX][coordinatesY].Green;
                rgbNewPixel[k][l].Blue =
                    physicalImage[coordinatesX][coordinatesY].Blue;
            }
            coordinatesY++;
        }
        coordinatesX++;
    }
    for(int k = 0; k < n; k++){
        for(int l = 0; l < n; l++){
            *newBlue += rgbNewPixel[k][l].Blue * filterMatrix[k][l];
            *newGreen += rgbNewPixel[k][l].Green * filterMatrix[k][l];
            *newRed += rgbNewPixel[k][l].Red * filterMatrix[k][l];
        }
    }
//verificarea daca noul pixel apartine parametrilor standard
    if(*newBlue < MIN_PIXEL_VALUE){
        *newBlue = MIN_PIXEL_VALUE;
    }
    if(*newRed < MIN_PIXEL_VALUE){
        *newRed = MIN_PIXEL_VALUE;
    }
    if(*newGreen < MIN_PIXEL_VALUE){
        *newGreen = MIN_PIXEL_VALUE; 
    }
    if(*newBlue > MAX_PIXEL_VALUE){
        *newBlue = MAX_PIXEL_VALUE;
    }
    if(*newRed > MAX_PIXEL_VALUE){
        *newRed = MAX_PIXEL_VALUE;
    }
    if(*newGreen > MAX_PIXEL_VALUE){
        *newGreen = MAX_PIXEL_VALUE;
    }
}

//functia care formeaza noua poza, aplicandu-se filtrul dat in enunt,
//apelandu-se functia newConvolutionalsPixels
void convolutionalLayers(bmp_image **physicalImage, bmp_fileheader *fileHeader,
    bmp_infoheader * infoHeader, char image[70], char filter[70]){

    image[strlen(image) - 4] = '\0';
    strcat(image,"_filter.bmp");
    FILE *out = fopen(image, "wb");

    FILE *filter_txt = fopen(filter, "rt");
    int n;
    fscanf(filter_txt, "%d", &n);
    int **filterMatrix = calloc(n, sizeof(int *));
    if(filterMatrix == NULL){
        exit(ERROR_CODE);
    }
    for(int i = 0; i < n; i++){
        filterMatrix[i] = calloc(n, sizeof(int));
        if(filterMatrix[i] == NULL){
            exit(ERROR_CODE);
        }
    }
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n; j++){
            fscanf(filter_txt, "%d", &filterMatrix[i][j]);
        }
    }
    fclose(filter_txt);
    print(out, fileHeader, infoHeader);

    for(int i = infoHeader -> height - 1; i >= 0; i--){
		for(int j = 0; j < infoHeader -> width; j++){
			bmp_image **rgbNewPixel = calloc(n, sizeof(bmp_image *));
			for(int k = 0; k < n; k++){
				rgbNewPixel[k] = calloc(n, sizeof(bmp_image));
			}
			
            int *newBlue = calloc(1, sizeof(int));
            if(newBlue == NULL){
                exit(ERROR_CODE);
            }
            int *newGreen = calloc(1, sizeof(int));
            if(newGreen == NULL){
                exit(ERROR_CODE);
            }
            int *newRed = calloc(1, sizeof(int));
            if(newBlue == NULL){
                exit(ERROR_CODE);
            }
            
            newConvolutionalPixels(physicalImage, rgbNewPixel,
                i, j, n, filterMatrix, newRed, newGreen, newBlue, infoHeader);

			fwrite(newBlue, sizeof(unsigned char), 1, out);
			fwrite(newGreen, sizeof(unsigned char), 1, out);
			fwrite(newRed, sizeof(unsigned char), 1, out);

			for(int k = 0; k < n; k++){
				free(rgbNewPixel[k]);
			}
			free(rgbNewPixel);
            free(newRed);
            free(newGreen);
            free(newBlue);
		}
		int padding = 0;
		fwrite(&padding, sizeof(unsigned char), infoHeader -> width % 4, out);
	}
	for(int i = 0; i < n; i++){
		free(filterMatrix[i]);
	}
	free(filterMatrix);
	fclose(out);
}

//functia care alege minimiul din codul rgb, pentru un pixel, formandu-se astfel
//un nou pixel
void minPool(int *newBlue, int *newGreen, int *newRed, bmp_image **rgbNewPixel,
    int n){

    *newBlue = MAX_PIXEL_VALUE;
    *newGreen = MAX_PIXEL_VALUE;
    *newRed = MAX_PIXEL_VALUE;
    for(int k = 0; k < n; k++){
        for(int l = 0; l < n; l++){
            if(*newBlue > rgbNewPixel[k][l].Blue){
                *newBlue = rgbNewPixel[k][l].Blue;
            }
            if(*newGreen > rgbNewPixel[k][l].Green){
                *newGreen = rgbNewPixel[k][l].Green;
            }
            if(*newRed > rgbNewPixel[k][l].Red){
                *newRed = rgbNewPixel[k][l].Red;
            }
        }
    }
}

//functia care alege maximul din codul rgb, pentru un pixel, formandu-se astfel
//un nou pixel
void maxPool(int *newBlue, int *newGreen, int *newRed, bmp_image **rgbNewPixel,
    int n){

    *newBlue = MIN_PIXEL_VALUE;
    *newGreen = MIN_PIXEL_VALUE;
    *newRed = MIN_PIXEL_VALUE;
    for(int k = 0; k < n; k++){
        for(int l = 0; l < n; l++){
            if(*newBlue < rgbNewPixel[k][l] . Blue){
                *newBlue = rgbNewPixel[k][l] . Blue;
            }
            if(*newGreen < rgbNewPixel[k][l] . Green){
                *newGreen = rgbNewPixel[k][l] . Green;
            }
            if(*newRed < rgbNewPixel[k][l] . Red){
                *newRed = rgbNewPixel[k][l] . Red;
            }
        }
    }
}

//functia care formeaza poza ceruta prin inlocuirea pixelilor initiali cu 
//cei obtinuti prin selectarea minimului/maximului, in functie de ce se cere
//in fisierul dat
void minMaxPooling(bmp_image **physicalImage, bmp_fileheader *fileHeader,
    bmp_infoheader * infoHeader, char image[70], char pooling[70]){
	
    image[strlen(image) - 4] = '\0';
    strcat(image,"_pooling.bmp");
    FILE *out = fopen(image, "wb");
    FILE *pooling_txt = fopen(pooling, "rt");
    int n;
    char selection;
    fscanf(pooling_txt, "%c", &selection);	
    fscanf(pooling_txt, "%d", &n);
    fclose(pooling_txt);
    print(out, fileHeader, infoHeader);

    for(int i = infoHeader->height - 1; i >= 0; i--){
        for(int j = 0; j < infoHeader->width; j++){
            bmp_image **rgbNewPixel = calloc(n, sizeof(bmp_image *));
            for(int k = 0; k < n; k++){
                rgbNewPixel[k] = calloc(n, sizeof(bmp_image));
            }
            int coordinatesX, coordinatesY, copy_coordinatesY;
            coordinatesX = i - n / 2;
            coordinatesY = j - n / 2;
            copy_coordinatesY = coordinatesY;
            
            //cazul in care pixelul nu se poate forma simplu, din cauza ca se
            //afla pe "marginea" pozei, sau aproape de margine
            for(int k = 0; k < n; k++){
                coordinatesY = copy_coordinatesY;
                for(int l = 0; l < n; l++){
                    if(coordinatesX < 0 || coordinatesY < 0 || coordinatesX >=
                        infoHeader->height || coordinatesY >= infoHeader->width){
                        rgbNewPixel[k][l].Red = MIN_PIXEL_VALUE;
                        rgbNewPixel[k][l].Green = MIN_PIXEL_VALUE;
                        rgbNewPixel[k][l].Blue = MIN_PIXEL_VALUE;
                    }
                    else{
                        rgbNewPixel[k][l].Red =
                            physicalImage[coordinatesX][coordinatesY].Red;
                        rgbNewPixel[k][l].Green =
                            physicalImage[coordinatesX][coordinatesY].Green;
                        rgbNewPixel[k][l].Blue =
                            physicalImage[coordinatesX][coordinatesY].Blue;
                    }
                    coordinatesY++;
                }
                coordinatesX++;
            }
            int *newBlue = calloc(1, sizeof(int));
            if(newBlue == NULL){
                exit(ERROR_CODE);
            }
            int *newGreen = calloc(1, sizeof(int));
            if(newGreen == NULL){
                exit(ERROR_CODE);
            }
            int *newRed = calloc(1, sizeof(int));
            if(newRed == NULL){
                exit(ERROR_CODE);
            }

//formarea noilor pixeli in functie de tipul de selectie aleasa
            if(selection == 'm'){
                minPool(newBlue, newGreen, newRed, rgbNewPixel, n);
            }
            if(selection == 'M'){
                maxPool(newBlue, newGreen, newRed, rgbNewPixel, n);
            }
            fwrite(newBlue, sizeof(unsigned char), 1, out);
            fwrite(newGreen, sizeof(unsigned char), 1, out);
            fwrite(newRed, sizeof(unsigned char), 1, out);

            for(int k = 0; k < n; k++){
                free(rgbNewPixel[k]);
            }
            free(rgbNewPixel);
            free(newBlue);
            free(newRed);
            free(newGreen);
        }
        int padding = 0;
        fwrite(&padding, sizeof(unsigned char), infoHeader->width % 4, out);
    }
    fclose(out);
}

//verificarea conditiei de a fi in cadrul pozei, necesara pentru functia de 
//umplere de la clustering
int verifyCondition(flood_fill *pixelInput, short i, short j){
    if((i >= 0 && j >= 0 && i < pixelInput->infoHeader->height && 
        j < pixelInput->infoHeader->width && pixelInput->matrixFrequence[i][j]
        == 0) && (abs(pixelInput->physicalImage[i][j].Red -
        pixelInput->pixelReference.Red) +
        abs(pixelInput->physicalImage[i][j].Blue -
        pixelInput->pixelReference.Blue) +
        abs(pixelInput->physicalImage[i][j].Green -
        pixelInput->pixelReference.Green) <= pixelInput->treshold)){

        pixelInput->matrixFrequence[i][j] = pixelInput->numberZone;
    return 1;
    }
    else{
        return 0;
    }
}

//functia de formare a zonelor, bazat pe algoritmul lui Lee sau fill, recursive
//mode
void floodFill(flood_fill *pixelInput, short i, short j){
	if(verifyCondition(pixelInput, i + 1, j) == 1){
        floodFill(pixelInput, i + 1, j);
    }
    if(verifyCondition(pixelInput, i - 1, j) == 1){
        floodFill(pixelInput, i - 1, j);
    }
    if(verifyCondition(pixelInput, i, j + 1) == 1){
        floodFill(pixelInput, i, j + 1);
    }
    if(verifyCondition(pixelInput, i, j - 1) == 1){
        floodFill(pixelInput, i, j - 1);
    }
}

//functia de clusterizare
void clustering(bmp_image **physicalImage, bmp_fileheader *fileHeader,
    bmp_infoheader * infoHeader, char image[70], char cluster[70]){
	
    //formarea antetului pozei
    image[strlen(image) - 4] = '\0';
    strcat(image,"_clustered.bmp");
    FILE *out = fopen(image, "wb");
    FILE *cluster_txt = fopen(cluster, "rt");
    int treshold;
    fscanf(cluster_txt, "%d", &treshold);
    fclose(cluster_txt);
    print(out, fileHeader, infoHeader);

//daca treshold-ul este 0, poza ramane neschimbata
    if(treshold == 0){
        print_physicalImage(physicalImage, out, fileHeader, infoHeader);
    }
    else{
        int **matrixFrequence = calloc(infoHeader -> height, sizeof(int *));
        for(int i = 0; i < infoHeader -> height; i++){
            matrixFrequence[i] = calloc(infoHeader -> width, sizeof(int));
        }
        for(int i = 0; i < infoHeader -> height; i++){
            for(int j = 0; j < infoHeader -> width; j++){
                matrixFrequence[i][j] = 0;
            }
        }
        flood_fill *pixelInput = calloc(1, sizeof(flood_fill));
        pixelInput -> fileHeader = fileHeader;
        pixelInput -> infoHeader = infoHeader;
        pixelInput -> physicalImage = physicalImage;
        pixelInput -> matrixFrequence = matrixFrequence;
        pixelInput -> treshold = treshold;
        pixelInput -> numberZone = 0;
//formarea zonelor din cadrul zonelor
        for(short i = 0; i < infoHeader -> height; i++){
            for(short j = 0; j < infoHeader -> width; j++){
                bmp_image pixelReference;
                if(matrixFrequence[i][j] == 0){
                    pixelInput -> numberZone++;
                    pixelInput -> pixelReference.Red =
                        physicalImage[i][j].Red;
                    pixelInput-> pixelReference.Green =
                        physicalImage[i][j].Green;
                    pixelInput -> pixelReference.Blue =
                        physicalImage[i][j].Blue;
                    pixelInput -> matrixFrequence[i][j] =
                        pixelInput -> numberZone;
                    floodFill(pixelInput, i, j);
                }
            }
        }
//dupa gasirea zonelor, pentru fiecare zona in parte vom afla pixelul
//corespuzator, calculat prin suma tuturor pixelilor din zona respectiva,
//impartit la numarul de pixeli din zona
        int *zoneSize = calloc(pixelInput->numberZone, sizeof(int));
        int *newRed = calloc(pixelInput->numberZone, sizeof(int));
        int *newGreen = calloc(pixelInput->numberZone, sizeof(int));
        int *newBlue = calloc(pixelInput->numberZone, sizeof(int));
        if(zoneSize == NULL || newRed == NULL || newGreen == NULL ||
            newBlue == NULL){
            
            exit(ERROR_CODE);
        }
        for(int i = 0; i < infoHeader->height; ++i){
            for(int j = 0; j < infoHeader->width; ++j){
                zoneSize[matrixFrequence[i][j] - 1]++;
                newRed[matrixFrequence[i][j] - 1] += physicalImage[i][j].Red;
                newGreen[matrixFrequence[i][j] - 1] += physicalImage[i][j].Green;
                newBlue[matrixFrequence[i][j] - 1] += physicalImage[i][j].Blue;
            }
        }
        for(int i = 0; i < pixelInput->numberZone; i++){
            newRed[i] /= zoneSize[i];
            newGreen[i] /= zoneSize[i];
            newBlue[i] /= zoneSize[i];
        }
//formarea noii poze prin inlocuirea pixelilor cu noua valoare per zona
        for(int i = 0; i < infoHeader -> height; i++){
            for(int j = 0; j < infoHeader -> width; j++){
                physicalImage[i][j].Red = newRed[matrixFrequence[i][j] - 1];
                physicalImage[i][j].Green = newGreen[matrixFrequence[i][j] - 1];
                physicalImage[i][j].Blue = newBlue[matrixFrequence[i][j] - 1];
            }
        }
        free(zoneSize);
        free(newRed);
        free(newBlue);
        free(newGreen);
        
        for(int i = 0; i < infoHeader -> height; i++){
            free(matrixFrequence[i]);
        }
        free(matrixFrequence);
        free(pixelInput);
        print_physicalImage(physicalImage, out, fileHeader, infoHeader);
    }
    fclose(out);
}	

int main(){
	
    bmp_fileheader *fileHeader = calloc(1,sizeof(bmp_fileheader));
    if(fileHeader == NULL){
        exit(ERROR_CODE);
    }
    bmp_infoheader *infoHeader = calloc(1,sizeof(bmp_infoheader));
    if(infoHeader == NULL){
        exit(ERROR_CODE);
    }	
    FILE *input_txt = fopen("./input.txt", "rt");

    char image[70], filter[70], pooling[70], cluster[70];
    fscanf(input_txt, "%s%s%s%s", image, filter, pooling, cluster);
    char copyImage[70];
    for(int i = 0; i <= strlen(image); i++){
        copyImage[i] = image[i];
    }
    FILE *inputImage = fopen(image, "rb");
    //citirea headerului pozei
    read_data(inputImage, fileHeader, infoHeader);

    bmp_image **physicalImage = calloc(infoHeader -> height,
        sizeof(bmp_image *));
    if(physicalImage == NULL){
        exit(ERROR_CODE);
    }
    //task 1
    for(int i = 0; i < infoHeader -> height; i++){
        physicalImage[i] = calloc(infoHeader -> width, sizeof(bmp_image));
        if(physicalImage[i] == NULL){
            exit(ERROR_CODE);
        }
    }
    read_physicalImage(physicalImage, inputImage, infoHeader, fileHeader);
    blackAndWhite(physicalImage, fileHeader, infoHeader, image);
    //task2
    for(int i = 0; i <= strlen(copyImage); i++){
        image[i] = copyImage[i];
    }
    read_physicalImage(physicalImage, inputImage, infoHeader, fileHeader);
    noCrop(physicalImage, fileHeader, infoHeader, image);
    //task 3
    for(int i = 0; i <= strlen(copyImage); i++){
        image[i] = copyImage[i];
    }
    read_physicalImage(physicalImage, inputImage, infoHeader, fileHeader);
    convolutionalLayers(physicalImage, fileHeader, infoHeader, image, filter);
    //task4
    for(int i = 0; i <= strlen(copyImage); i++){
        image[i] = copyImage[i];
    }
    read_physicalImage(physicalImage, inputImage, infoHeader, fileHeader);
    minMaxPooling(physicalImage, fileHeader, infoHeader, image, pooling);
    //task5
    for(int i = 0; i <= strlen(copyImage); i++){
        image[i] = copyImage[i];
    }
    read_physicalImage(physicalImage, inputImage, infoHeader, fileHeader);
    clustering(physicalImage, fileHeader, infoHeader, image, cluster);

    for(int i = 0; i < infoHeader -> height; ++i){
        free(physicalImage[i]);	
    }
    free(physicalImage);
    free(fileHeader);
    free(infoHeader);
    fclose(inputImage);
    return 0;
}