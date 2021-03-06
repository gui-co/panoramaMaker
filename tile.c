/*****************************************************************************
 * tile.c                                               .       .          . *
 * Methodes to deal with DTED files and tiles, the    .   .   /\   .     .   *
 * memory representation of a DTED file                .     /  \ .   /\    .*
 *                                                   .    . /    \/\ /  \  . *
 * panoramaMaker                                      .    /     /  \    \   *
 * Guillaume Communie - guillaume.communie@gmail.com      /     /    \    \  *
 *****************************************************************************/


#include "tile.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <string.h>


/*
 * Conversion of altitude from DTED file representation to little endian 
 * 2's complement representation.
 * param x: DTED file data.
 * return: Data in little endian and negative in 2's complement.
 */
static int16_t toLittleEndian(int16_t x)
{
  x = ((uint16_t) x >> 8) | ((uint16_t) x << 8);
  x = x & 0x8000 ? -1 * (x & 0x7FFF) : x;
  return x;
}


/*
 * Free the memory associated to a tile.
 * param tile: Pointer to a tile.
 */
void freeTile(Tile* tile)
{
  free(tile->data);
  free(tile);
}


/* 
 * Open a DTED file, read and store the altitude data.
 * param fileName: Path to a DTED file.
 * return: Pointer to the tile.
 */
Tile* openTile(char* fileName)
{
  FILE* dtedFile;
  long fileSize, expectedSize;
  char headerData[10];
  Tile* tile;
  int nbLon, nbLat, pt, coverage;

  // Open file.
  if (!(dtedFile = fopen(fileName, "r")))
  {
    errPrintf("Unable to open %s >", fileName);
  }
    
  fprintf(stdout, "Opening of %s\n", fileName);

  // Get the number of points.
  fseek(dtedFile, DTED_DSI_NBLAT, SEEK_SET);
  fread(&headerData, 4, 1, dtedFile);
  nbLat = 1000 * (headerData[0] - 48) + 100 * (headerData[1] - 48) 
          + 10 * (headerData[2] - 48) + (headerData[3] - 48);
  fseek(dtedFile, DTED_DSI_NBLON, SEEK_SET);
  fread(&headerData, 4, 1, dtedFile);
  nbLon = 1000 * (headerData[0] - 48) + 100 * (headerData[1] - 48) 
          + 10 * (headerData[2] - 48) + (headerData[3] - 48);
  
  // Check file.
  fseek(dtedFile, 0, SEEK_END);
  fileSize = ftell(dtedFile);
  fprintf(stdout, "Size: %ld bytes\n", fileSize);
  expectedSize = DTED_DATA + (nbLat * 2 + 12) * nbLon;
  if (fileSize != expectedSize)
  {
    errPrintf("The file has an incorrect size. %ld bytes where expected", 
        expectedSize);
  }
  fseek(dtedFile, DTED_DSI_DATACOV, SEEK_SET);
  fread(&headerData, 2, 1, dtedFile);
  coverage = 10 * (headerData[0] - 48) + (headerData[1] - 48);
  coverage = coverage ? coverage : 100;
  printf("File covers %d%% of the region.\n", coverage); 

  // Memory allocation.
  if (!(tile = malloc(sizeof(Tile))))
  {
    errPrintf("Unable to allocate memory for the tile >");
  }
  if (!(tile->data = malloc(nbLat * nbLon * sizeof(uint16_t))))
  {
    errPrintf("Unable to allocate memory for the tile data >");
  }

  // Data storage.
  tile->latitudeDimension = nbLat;
  tile->longitudeDimension = nbLon;
  fseek(dtedFile, 3428, SEEK_SET);
  for (int i = 0 ; i < nbLon ; i++)  // Longitude goes W to E.
  {
    fseek(dtedFile, 8, SEEK_CUR);
    for (int j = 0 ; j < nbLat ; j++)  // Lat goes S to N.
    {
      pt = i * nbLon + j;
      fread(&tile->data[pt], 2, 1, dtedFile);
      tile->data[pt] = toLittleEndian(tile->data[pt]);
    }
    fseek(dtedFile, 4, SEEK_CUR);
  }
  
  fclose(dtedFile);
  return tile;
}


/*
 * Create a space, data structure that will contain the tiles around the
 * origin.
 * param originLon: longitude of the origin, center of the space.
 * param originLat: latitude of the origin, center of the space.
 * return: pointer to the space.
 */
Space* initSpace(double originLon, double originLat)
{
  Space* space;
  char fileName[35];
  char lonDir;
  char latDir;
  int lonDeg;
  int latDeg;

  // Init Space
  if (!(space = malloc(sizeof(Space))))
  {
    errPrintf("Unable to allocate memory for the space >");
  }
  space->originLon = originLon;
  space->originLat = originLat;
  memset(space->tiles, 0, SIZE_SPACE * sizeof(Tile*));

  // Open the DTED file corresponding to origin
  lonDir = originLon > 0 ? 'e' : 'w';
  latDir = originLat > 0 ? 'n' : 's';
  lonDeg = (int) originLon;
  lonDeg = lonDeg > 0 ? lonDeg : -(lonDeg - 1);
  latDeg = (int) originLat;
  latDeg = latDeg > 0 ? latDeg : -(latDeg - 1);
  sprintf(fileName, DATA_DIR "%c%02d_%c%03d_1arc_v3.dt2",
      latDir, latDeg, lonDir, lonDeg);
  space->tiles[SIZE_SPACE / 2] = openTile(fileName);

  return space;
}


/*
 * Free space associated to a space.
 * param space: the space to be freed
 */
void freeSpace(Space* space)
{
  for (int i = 0 ; i < SIZE_SPACE ; i++)
  {
    if (space->tiles[i])
    {
      freeTile(space->tiles[i]);
    }
  }
  free(space);
}

