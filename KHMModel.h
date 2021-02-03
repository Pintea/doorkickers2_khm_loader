#pragma once

#include "Kernel/CommonDefs.h"
#include "Kernel/Matrix.h"
#include "Kernel/Vector.h"
#include "Kernel/Quat.h"
#include "Kernel/HashedString.h"
#include "Common/Collision.h"

struct sCollisionPolygon
{
	Vector3					normal; // normal, -d
	float					d;
	short					numIndices;
	short					indexStart; // index into collision.mesh.pIndices
};

struct sCollisionMesh
{
	sCollisionPolygon*		pPolygons;
	unsigned short*			pIndices;
	Vector3*				pVertices;
	int						numPolys;
	int						numIndices; // triangle indices
	int						numVertices;
};

struct sCollisionShape
{
	enum eCollisionType
	{
		SPHERE,
		BOX,
		CAPSULE,
		CONVEX_MESH,
		MESH, // triangle mesh, assumes concave. Will be cooked into a convex shape at runtime.
		PLANE
	};

	sCollisionShape()
	{
		type = SPHERE;
		bShared = false;
		params.sphere.radius = 0.0f;
		transform.Identity();
		params.mesh.pVertices = NULL;
		params.mesh.pIndices = NULL;
		params.mesh.pPolygons = NULL;
	}

	void Destroy()
	{
		if (type == CONVEX_MESH)
		{
			delete [] params.mesh.pPolygons;
			delete [] params.mesh.pIndices;
			delete [] params.mesh.pVertices;
		}
		type = SPHERE;
	}

	eCollisionType			type;
	Matrix					transform; // not used for meshes
	bool					bShared; // valid only for convex meshes & meshes, defaults to "true" for KHM models. Used only for physics/collision detection for now.

	union
	{
		struct 
		{
			float			radius;
		} sphere;

		struct
		{
			float			extents[3];
		} box;

		struct
		{
			float			radius;
			float			halfHeight; // elongates the sphere defined by 'radius' on the X axis. If zero, it's a sphere. (see PhysX manual)
		} capsule;

		// planes default on +X orientation normal=(1,0,0). Rotate/displace using the collision transform
		struct
		{

		} plane;

		sCollisionMesh		mesh;
	} params;
};

namespace KHM
{
	//
	// common defines for KHM
	//

	#define KHM_VERSION						101
	#define KHM_MAX_OBJECT_NAME				48
	#define KHM_MAX_BONE_INFLUENCES			4 // max bone influences per vertex
	#define KHM_MAX_BONES					64

	//
	// header for KHM model
	//

	struct sHeader
	{
		sHeader()
		{
			uiSig[0] = 'K';
			uiSig[1] = 'H';
			uiSig[2] = 'M';
			uiSig[3] = '\0';

			uiVer = KHM_VERSION;
		}

		unsigned char			uiSig[4]; // file signature; static; 'KHM\0'
		unsigned int			uiVer; // exporter model version
	};


	//
	// KHM Object Base ( common stuff for all objects )
	//

	struct sObjectBase // keep 4-byte aligned
	{
		char					szName[KHM_MAX_OBJECT_NAME];

		unsigned int			uiId;
		unsigned int			uiParentId;

		// TODO: we only need 3x4 mat
		Matrix					matLocal;
		Matrix					matGlobal;
	};

	//
	// KHM Bone Indices
	//

	struct sBoneIndices // keep 4-byte aligned
	{
		unsigned char			ind[KHM_MAX_BONE_INFLUENCES];
	};

	//
	// KHM Mesh ( object with geometry )
	//

	struct sObjectMesh : public sObjectBase
	{
		sObjectMesh()
		{
			numVertices			= 0;
			pVertices			= NULL;
			pNormals			= NULL;
			pColors				= NULL;
			pTexCoords[0]		= NULL;
			pTexCoords[1]		= NULL;
			pSkinWeights		= NULL;
			pSkinBoneIndices	= NULL;
			numIndices			= 0;
			pIndices			= NULL;
			pFaceNormals		= NULL;
			numCollisions		= 0;
			pCollisions			= NULL;
			volume				= 0.0f;
		}

		void Delete()
		{
			delete [] pVertices;
			delete [] pNormals;
			delete [] pColors;
			delete [] pTexCoords[0];
			delete [] pTexCoords[1];
			delete [] pSkinWeights;
			delete [] pSkinBoneIndices;
			delete [] pIndices;
			delete [] pFaceNormals;

			for (int i = 0; i < numCollisions; ++i)
				pCollisions[i].Destroy();
			delete [] pCollisions;
		}

		int						numVertices;
		Vector3*				pVertices;		// positions; in global space
		Vector3*				pNormals;
		unsigned int*			pColors;		// vertex colors
		Vector2*				pTexCoords[2];	// texture coordinates; 2 sets max
			
		// work in pair; 1 from lSkinWeights -> 1 from lSkinBoneIndices
		Vector4*				pSkinWeights;
		sBoneIndices*			pSkinBoneIndices;

		int						numIndices;
		unsigned short*			pIndices;		// triangle indices
		Vector3*				pFaceNormals;	// per-triangle normals

		int						numCollisions;	// precise collisions for the object, using collision primitives
		sCollisionShape*		pCollisions;

		Vector3					min;			// precomputed bounds for the entire model
		Vector3					max;
		float					volume;			// computed at load time, should export
	};

	//
	// KHM Bone Transform - animation frame for a bone / time
	//

	struct sNodeTransform // keep 4-byte aligned
	{
		Quat					qRot;		// rotation stored as a quaternion
		Vector3					vTrans;		// translation
		Vector3					vScale;		// scale
	};

	//
	// KHM Bone Animation - entire animation for a bone
	//

	// TODO: this should only be needed for debugging purposes
	struct sNodeAnimation
	{
		unsigned int			uiNodeId;			// node id, as it was generated at export time
		char					szNodeName[KHM_MAX_OBJECT_NAME];	// actual node's name
	};

	//
	// KHM Animation Mask
	//

	// TODO: could be made better for runtime, since we only access the object names once, but the mask itself many times per frame
	//  we should either keep a bitset or a bool masks[numBones]. The names should be kept in a different array, only for debugging purposes
	struct sAnimationMaskEntry // keep 4-byte aligned
	{
		char					szObjectName[KHM_MAX_OBJECT_NAME]; // object masked
		int						mask; // true or false; 1 - use object; 0 - mask object
	};

	struct sAnimationMask
	{
		~sAnimationMask()
		{
			delete[] pNodes;
		}

		sAnimationMaskEntry*	pNodes;
		int						numNodes;
	};

	//
	// KHM Animation Data
	//


	struct sAnimation
	{
		~sAnimation()
		{
			//delete [] pNodeAnimations;
			delete [] pNodeTransforms;
		}

		// TODO: this layout doesn't make much sense. 
		//  numNodeAnimations should be equal to the number of bones, so not needed here. 
		//  pNodeAnimations is definitely not needed, maybe only for debugging purposes 

		//sNodeAnimation*		pNodeAnimations = NULL; // not used anymore
		sNodeTransform*			pNodeTransforms = {};	// size = (numNodeFrames * numNodes)
		int						numNodes;				//NOTE: This isn't necessarily equal to the number of nodes ( because of attachments )
		int						numNodeFrames;			// number of frames per node
		float					frameDurationMs;		// single frame duration in millis
	};

	//
	// KHM Model Definition
	//

	struct sModelDefinition
	{
		sModelDefinition()
		{
			pMesh = NULL;
			numHelpers = 0;
			lHelpers = NULL;
			numBones = 0;
			lBones = NULL;
			pAnimation = NULL;
			pAnimationMask = NULL;
			membuff = NULL;
		}

		~sModelDefinition()
		{
			if (pMesh)
			{
				if (membuff)
				{
					delete[] pMesh->pCollisions;
				}
				else
				{
					pMesh->Delete();
				}
			}
			delete pMesh;

			if (membuff)
			{
				lBones = NULL;
				lHelpers = NULL;

				if (pAnimation)
					pAnimation->pNodeTransforms = NULL;
				if (pAnimationMask)
					pAnimationMask->pNodes = NULL;
			}

			delete[] lBones;
			delete[] lHelpers;
			delete pAnimation;
			delete pAnimationMask;

			delete[] membuff;
		}

		const sObjectBase*		GetObjectById( const unsigned int uiId ) const;
		const sObjectBase*		GetObjectByName( const char* pszName ) const;

		HashedString			sModelName;		// original filename for the scene
		sObjectMesh*			pMesh;			// list of all the meshes
		int						numHelpers;
		sObjectBase*			lHelpers;		// list with all the helpers
		int						numBones;
		sObjectBase*			lBones;			// list with all the bones
		sAnimation*				pAnimation;		// animations in the KHM file
		sAnimationMask*			pAnimationMask;	// masks for this animation; to remove this ! and move it to the skeleton / animation manager

		unsigned char*			membuff;		// memory for the entire structure. All members here point to chunks from this buffer. This is actually the entire khm file itself.
	};

	//
	// KHM File Loader
	//

	class CLoader
	{
		public:
			CLoader();

		public:
			sModelDefinition* LoadModel(const char* pszFilePath);

		private:
			void ReadAnimation( sModelDefinition* pModelDefinition );
			void ReadAnimationMask( sModelDefinition* pModelDefinition );
			
			void ReadMeshes( sModelDefinition* pModelDefinition );
			void ReadBones( sModelDefinition* pModelDefinition );
			void ReadHelpers( sModelDefinition* pModelDefinition );

			void ReadGeometry( sObjectMesh* pMesh );
			void ReadSkin( sObjectMesh* pMesh );
			void ReadCollisionData( sObjectMesh* pMesh );
			void ReadText( char* szBuffer, unsigned int uiBufferLen );
			void ReadBytes( void* pDest, int sizeToRead );
			unsigned char* ReadBytes(int sizeToRead);
			void ReadBytesSkip( int numBytesSkip );

	private:
			unsigned char* buff;
			unsigned int buffsize;
			int buffread;
	};
};
