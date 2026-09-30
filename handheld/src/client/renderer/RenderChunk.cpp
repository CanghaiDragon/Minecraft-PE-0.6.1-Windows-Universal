#include "RenderChunk.h"

int RenderChunk::runningId = 0;

RenderChunk::RenderChunk() :
	vboId(-1),
	vertexCount(0),
	clientData(NULL)
{
	id = ++runningId;
}

RenderChunk::RenderChunk( GLuint vboId_, int vertexCount_ )
:	vboId(vboId_),
	vertexCount(vertexCount_),
	clientData(NULL)
{
	id = ++runningId;
}
