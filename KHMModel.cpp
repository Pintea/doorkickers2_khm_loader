
#include "KHMModel.h"
#include "Kernel/Log.h"
#include "Kernel/FileManager.h"

namespace KHM {

//
// sModelDefinition
//

const sObjectBase* sModelDefinition::GetObjectById( const unsigned int uiId ) const
{
	// look through meshes
	if( pMesh && pMesh->uiId == uiId )
		return pMesh;

	// look through bones
	for( int i(0); i < numBones; ++i )
	{
		const sObjectBase& pObj_i = lBones[ i ];
		if( pObj_i.uiId == uiId )
			return &pObj_i;
	}

	// look through helpers
	for( int i(0); i < numHelpers; ++i )
	{
		const sObjectBase& pObj_i = lHelpers[ i ];
		if( pObj_i.uiId == uiId )
			return &pObj_i;
	}

	return NULL;
}

const sObjectBase* sModelDefinition::GetObjectByName( const char* pszName ) const
{
	// look through meshes
	if( pMesh && strcmp( pMesh->szName, pszName ) == 0 )
		return pMesh;

	// look through bones
	for( int i(0); i < numBones; ++i )
	{
		const sObjectBase& pObj_i = lBones[ i ];
		if( strcmp( pObj_i.szName, pszName ) == 0 )
			return &pObj_i;
	}

	// look through helpers
	for( int i(0); i < numHelpers; ++i )
	{
		const sObjectBase& pObj_i = lHelpers[ i ];
		if( strcmp( pObj_i.szName, pszName ) == 0 )
			return &pObj_i;
	}

	return NULL;
}

//
// CLoader
//

CLoader::CLoader() :
	buff(0),
	buffsize(0),
	buffread(0)
{
}

void CLoader::ReadBytesSkip(int numBytesSkip)
{
	buffread += numBytesSkip;
	//bool result = m_pFile->Seek(numBytesSkip, File::SEEK_POS_CUR);
	//ASSERT(result);
}

unsigned char* CLoader::ReadBytes(int sizeToRead)
{
	buffread += sizeToRead;
	return (buff + buffread - sizeToRead);
}

void CLoader::ReadBytes( void* pDest, int sizeToRead )
{
	pDest = buff + buffread;
	buffread += sizeToRead;
	ASSERT(buffread <= (int)buffsize);

	//unsigned int bytesRead = m_pFile->Read(pDest, sizeToRead);
	//ASSERT(bytesRead == (unsigned int)sizeToRead);
}

void CLoader::ReadText( char* szBuffer, unsigned int uiBufferLen )
{
	unsigned int uiTextLen;
	ReadBytes( &uiTextLen, sizeof( uiTextLen ) );

	memset( szBuffer, 0x00, uiBufferLen * sizeof(char) );
	ASSERT( uiTextLen < uiBufferLen );
	if( uiTextLen < uiBufferLen )
	{
		ReadBytes( szBuffer, uiTextLen );
	}
	else
	{
		ReadBytes( szBuffer, uiBufferLen );
		szBuffer[ uiBufferLen - 1 ] = '\0';

		char tempBuff[1024];
		ReadBytes(tempBuff, uiTextLen - uiBufferLen);
	}
}

void CLoader::ReadSkin( sObjectMesh* pMesh )
{
	const unsigned char hasSkin = *(unsigned char*)ReadBytes(sizeof(hasSkin));
	if (!hasSkin)
		return; // no skin

	pMesh->pSkinWeights = (Vector4*)ReadBytes(sizeof(Vector4) * pMesh->numVertices);
	pMesh->pSkinBoneIndices = (sBoneIndices*)ReadBytes(sizeof(sBoneIndices) * pMesh->numVertices);
}

void CLoader::ReadCollisionData( sObjectMesh* pMesh )
{
	pMesh->numCollisions = *(int*)ReadBytes(sizeof(int));
	if (!pMesh->numCollisions)
		return;

	pMesh->pCollisions = new sCollisionShape[pMesh->numCollisions];
	for (int i = 0; i < pMesh->numCollisions; ++i)
	{
		sCollisionShape& col = pMesh->pCollisions[i];

		col.type = (sCollisionShape::eCollisionType)*(unsigned int*)ReadBytes(sizeof(unsigned int));
		memcpy(&col.transform, ReadBytes(sizeof(float) * 16), sizeof(float) * 16);

		switch (col.type)
		{
			case sCollisionShape::SPHERE:
				col.params.sphere.radius = *(float*)ReadBytes(sizeof(float));
				break;

			case sCollisionShape::BOX:
				memcpy(col.params.box.extents, ReadBytes(sizeof(float) * 3), sizeof(float) * 3);
				break;

			case sCollisionShape::CAPSULE:
				col.params.capsule.radius = *(float*)ReadBytes(sizeof(float));
				col.params.capsule.halfHeight = *(float*)ReadBytes(sizeof(float));
				break;

			case sCollisionShape::CONVEX_MESH:
				col.bShared = true;

				col.params.mesh.numPolys = *(int*)ReadBytes(sizeof(int));
				col.params.mesh.pPolygons = (sCollisionPolygon*)ReadBytes(sizeof(sCollisionPolygon) * col.params.mesh.numPolys);

				col.params.mesh.numIndices = *(int*)ReadBytes(sizeof(int));
				col.params.mesh.pIndices = (unsigned short*)ReadBytes(sizeof(unsigned short) * col.params.mesh.numIndices);

				col.params.mesh.numVertices = *(int*)ReadBytes(sizeof(int));
				col.params.mesh.pVertices = (Vector3*)ReadBytes(sizeof(Vector3) * col.params.mesh.numVertices);
				break;

			default:
				DEBUG_BREAK();
				break;
		}
	}
}

void CLoader::ReadGeometry(sObjectMesh* pMesh)
{
	// read verts
	pMesh->numVertices = *(int*)ReadBytes(sizeof(int));
	pMesh->pVertices = (Vector3*)ReadBytes(sizeof(Vector3) * pMesh->numVertices);

	// read normals
	pMesh->pNormals = (Vector3*)ReadBytes(sizeof(Vector3) * pMesh->numVertices);

	// read triangle indices
	pMesh->numIndices = *(int*)ReadBytes(sizeof(int));
	pMesh->pIndices = (unsigned short*)ReadBytes(sizeof(unsigned short) * pMesh->numIndices);

	// read face normals
	const unsigned int uiNumFaces = pMesh->numIndices / 3;
	pMesh->pFaceNormals = (Vector3*)ReadBytes(sizeof(Vector3) * uiNumFaces);

	// read vtx colors
	unsigned char hasVertColors = *(unsigned char*)ReadBytes(sizeof(hasVertColors));
	if (hasVertColors)
	{
		pMesh->pColors = (unsigned int*)ReadBytes(sizeof(unsigned int) * pMesh->numVertices);
	}

	// read tx coords
	unsigned int uiNumTxCoordMaps = *(unsigned int*)ReadBytes(sizeof(uiNumTxCoordMaps));
	for(unsigned int i(0); i < uiNumTxCoordMaps; ++i)
	{
		if (i == 1)
		{
			ReadBytesSkip(sizeof(Vector2) * pMesh->numVertices);
			//m_pFile->Seek(sizeof(Vector2) * pMesh->numVertices, File::SEEK_POS_CUR);
			continue; // TODO: we don't need these for now,  will see if necessary
		}

		pMesh->pTexCoords[i] = (Vector2*)ReadBytes(sizeof(Vector2) * pMesh->numVertices);
	}

	// read skin
	ReadSkin(pMesh);

	// read collision data
	ReadCollisionData(pMesh);

	// read bounds
	pMesh->min = *(Vector3*)ReadBytes(sizeof(Vector3));
	pMesh->max = *(Vector3*)ReadBytes(sizeof(Vector3));

	// compute volume at load time (TODO: this is exporter's job)
	pMesh->volume = 0.0f;
	for (int i = 0; i < pMesh->numCollisions; ++i)
	{
		const sCollisionShape& col = pMesh->pCollisions[i];
		if (col.type == sCollisionShape::CONVEX_MESH || col.type == sCollisionShape::MESH)
		{
			Vector3 s = pMesh->max - pMesh->min;
			pMesh->volume += (s.x * s.y * s.z);
		}
		else
		{
			pMesh->volume += col.GetVolume();
		}
	}
}

void CLoader::ReadMeshes( sModelDefinition* pModelDefinition )
{
	const unsigned char hasMesh = *ReadBytes(sizeof(hasMesh));
	if (!hasMesh)
		return;

	pModelDefinition->pMesh = new sObjectMesh();

	sObjectBase* pObject = pModelDefinition->pMesh;
	memcpy(pObject, ReadBytes(sizeof(sObjectBase)), sizeof(sObjectBase));

	ASSERT(pObject->uiId < 256);
	if (pObject->uiId >= 256) 
		pObject->uiId = 255;
	if (pObject->uiParentId > 256 && pObject->uiParentId < (unsigned)-1)
	{
		ASSERT(false);
		pObject->uiParentId = 255;
	}

	ReadGeometry(pModelDefinition->pMesh);

	//g_pLog->Write("mesh: %s, id=%d, parentId=%d\n", pObject->szName, pObject->uiId, pObject->uiParentId);
}

void CLoader::ReadBones( sModelDefinition* pModelDefinition )
{
	const unsigned char count = *(unsigned char*)ReadBytes(sizeof(count));
	if (!count)
		return;

	ASSERT(sizeof(sObjectBase) == (KHM_MAX_OBJECT_NAME + 4 + 4 + 64 + 64));
	pModelDefinition->numBones = count;
	pModelDefinition->lBones = (sObjectBase*)ReadBytes(sizeof(sObjectBase) * count);

	//for (int i = 0; i < pModelDefinition->numBones; ++i) {
	//	const KHM::sObjectBase* pBone = &pModelDefinition->lBones[i];
	//	g_pLog->Write("bone: %s, id=%d, parentId=%d\n", pBone->szName, pBone->uiId, pBone->uiParentId);
	//}
}

void CLoader::ReadHelpers( sModelDefinition* pModelDefinition )
{
	const unsigned char count = *(unsigned char*)ReadBytes(sizeof(count));
	if (!count)
		return;

	ASSERT(sizeof(sObjectBase) == (KHM_MAX_OBJECT_NAME + 4 + 4 + 64 + 64));
	pModelDefinition->numHelpers = count;
	pModelDefinition->lHelpers = (sObjectBase*)ReadBytes(sizeof(sObjectBase) * count);

	//for (int i = 0; i < pModelDefinition->numHelpers; ++i) {
	//	const KHM::sObjectBase* pBone = &pModelDefinition->lHelpers[i];
	//	g_pLog->Write("helper: %s, id=%d, parentId=%d\n", pBone->szName, pBone->uiId, pBone->uiParentId);
	//}
}

void CLoader::ReadAnimation( sModelDefinition* pModelDefinition )
{
	const unsigned char hasAnim = *ReadBytes(sizeof(hasAnim));
	if (!hasAnim)
		return;

	sAnimation* pAnimation = new sAnimation();
	pModelDefinition->pAnimation = pAnimation;

	// read data
	const int numNodes = *(int*)ReadBytes(sizeof(int));
	const float startTimeS = *(float*)ReadBytes(sizeof(float));
	const float endTimeS = *(float*)ReadBytes(sizeof(float));
	const int numFrames = *(int*)ReadBytes(sizeof(int));
	ASSERT(startTimeS == 0.f);
	if (startTimeS != 0.f) {
		g_pLog->Write("[ERROR] Found non-zero StartTime when loading animation %s. Please export entire animation.\n", pModelDefinition->sModelName.GetString());
		return;
	}

	// alloc space for node animations; node animations = animation track for a object
	pAnimation->numNodes = numNodes;

	// no longer needed (TODO: remove from exporter as well)
	ReadBytesSkip(sizeof(sNodeAnimation) * numNodes);
	//pAnimation->pNodeAnimations = new sNodeAnimation[numNodes];
	//ReadBytes(pAnimation->pNodeAnimations, sizeof(sNodeAnimation) * numNodes);

	pAnimation->numNodeFrames = numFrames;
	pAnimation->frameDurationMs = (endTimeS * 1000.f) / (float)(Max(numFrames - 1, 1));
	pAnimation->pNodeTransforms = (sNodeTransform*)ReadBytes(sizeof(sNodeTransform) * (numFrames * numNodes));

	//for (int i = 0; i < numNodes; ++i) {
	//	const KHM::sNodeAnimation* pNode = &pAnimation->pNodeAnimations[i];
	//	g_pLog->Write("anim: %s, id=%d\n", pNode->szNodeName, pNode->uiNodeId);
	//}
}

void CLoader::ReadAnimationMask( sModelDefinition* pModelDefinition )
{
	const unsigned char hasAnimMask = *ReadBytes(sizeof(hasAnimMask));
	if (!hasAnimMask)
		return;

	sAnimationMask* pMask = new sAnimationMask();
	pModelDefinition->pAnimationMask = pMask;

	const unsigned int numNodes = *(unsigned int*)ReadBytes(sizeof(unsigned int));
	
	pMask->pNodes = (sAnimationMaskEntry*)ReadBytes(sizeof(sAnimationMaskEntry) * numNodes);
	pMask->numNodes = numNodes;	

	//for (int i = 0; i < numNodes; ++i) {
	//	if (pNewAnimationMask->lObjectMask[i].mask)
	//		g_pLog->Write("mask: %s\n", pNewAnimationMask->lObjectMask[i].szObjectName);
	//}
}

sModelDefinition* CLoader::LoadModel(const char* pszFilePath)
{
	if (!pszFilePath)
		return NULL; // invalid input data

	buff = g_fileManager->FileLoadBinary(pszFilePath, &buffsize);
	if (!buff)
		return NULL; // could not open file

	// read file signature
	sHeader* fileHeader = (sHeader*)ReadBytes(sizeof(sHeader));

	if( fileHeader->uiSig[0] != 'K' ||
		fileHeader->uiSig[1] != 'H' ||
		fileHeader->uiSig[2] != 'M' )
	{
		LOG_ERROR("[Error] CLoader::LoadModel(%s) - KHM header mismatch.\n", pszFilePath);
		return NULL; // different kind of file
	}

	if( fileHeader->uiVer != KHM_VERSION )
	{
		LOG_ERROR("[Error] CLoader::LoadModel(%s) - wrong file version %u, expected %u\n", pszFilePath, fileHeader->uiVer, KHM_VERSION);
		return NULL; // version mismatch!
	}

	KHM::sModelDefinition* pModelDefinition = new KHM::sModelDefinition();
	pModelDefinition->membuff = buff;

	pModelDefinition->sModelName.CreateHash(pszFilePath, true);

	//
	// read bones
	ReadBones(pModelDefinition);	

	//
	// read helpers
	ReadHelpers(pModelDefinition);
	
	//
	// read meshes
	ReadMeshes(pModelDefinition);

	//
	// read animation
	ReadAnimation(pModelDefinition);

	//
	// read animation mask
	ReadAnimationMask(pModelDefinition);

	return pModelDefinition;
}

}; // end namespace KHM
