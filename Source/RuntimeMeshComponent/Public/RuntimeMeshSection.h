// Copyright 2016 Chris Conway (Koderz). All Rights Reserved.

#pragma once

#include "Engine.h"
#include "Components/MeshComponent.h"
#include "RuntimeMeshProfiling.h"
#include "RuntimeMeshVersion.h"
#include "RuntimeMeshSectionProxy.h"
#include "RuntimeMeshBuilder.h"
#include "RuntimeMeshLibrary.h"

/** Interface class for a single mesh section */
class FRuntimeMeshSectionInterface
{
protected:
	const bool bNeedsPositionOnlyBuffer;

public:
	/** Position only vertex buffer for this section */
	TArray<FVector> PositionVertexBuffer;

	/** Index buffer for this section */
	TArray<int32> IndexBuffer;

	/** Index buffer used for tessellation containing the needed adjacency info */
	TArray<int32> TessellationIndexBuffer;

	/** Local bounding box of section */
	FBox LocalBoundingBox;

	/** Should we build collision data for triangles in this section */
	bool CollisionEnabled;

	/** Should we display this section */
	bool bIsVisible;

	/** Should this section cast a shadow */
	bool bCastsShadow;

	/** If this section is currently using an adjacency index buffer */
	bool bShouldUseAdjacencyIndexBuffer;

	/** Update frequency of this section */
	EUpdateFrequency UpdateFrequency;

	FRuntimeMeshSectionInterface(bool bInNeedsPositionOnlyBuffer) : 
		bNeedsPositionOnlyBuffer(bInNeedsPositionOnlyBuffer),
		LocalBoundingBox(0),
		CollisionEnabled(false),
		bIsVisible(true),
		bCastsShadow(true),
		bIsInternalSectionType(false)
	{}

	virtual ~FRuntimeMeshSectionInterface() { }
	
protected:

	/** Is this an internal section type. */
	bool bIsInternalSectionType;

	bool IsDualBufferSection() const { return bNeedsPositionOnlyBuffer; }

	/* Updates the vertex position buffer,   returns whether we have a new bounding box */
	bool UpdateVertexPositionBuffer(TArray<FVector>& Positions, const FBox* BoundingBox, bool bShouldMoveArray)
	{
		// Holds the new bounding box after this update.
		FBox NewBoundingBox(0);

		if (bShouldMoveArray)
		{
			// Move buffer data
			PositionVertexBuffer = MoveTemp(Positions);

			// Calculate the bounding box if one doesn't exist.
			if (BoundingBox == nullptr)
			{
				for (int32 VertexIdx = 0; VertexIdx < PositionVertexBuffer.Num(); VertexIdx++)
				{
					NewBoundingBox += PositionVertexBuffer[VertexIdx];
				}
			}
			else
			{
				// Copy the supplied bounding box instead of calculating it.
				NewBoundingBox = *BoundingBox;
			}
		}
		else
		{
			if (BoundingBox == nullptr)
			{
				// Copy the buffer and calculate the bounding box at the same time
				int32 NumVertices = Positions.Num();
				PositionVertexBuffer.SetNumUninitialized(NumVertices);
				for (int32 VertexIdx = 0; VertexIdx < NumVertices; VertexIdx++)
				{
					NewBoundingBox += Positions[VertexIdx];
					PositionVertexBuffer[VertexIdx] = Positions[VertexIdx];
				}
			}
			else
			{
				// Copy the buffer
				PositionVertexBuffer = Positions;

				// Copy the supplied bounding box instead of calculating it.
				NewBoundingBox = *BoundingBox;
			}
		}

		// Update the bounding box if necessary and alert our caller if we did
		if (!(LocalBoundingBox == NewBoundingBox))
		{
			LocalBoundingBox = NewBoundingBox;
			return true;
		}

		return false;
	}

	void UpdateIndexBuffer(TArray<int32>& Triangles, bool bShouldMoveArray)
	{
		if (bShouldMoveArray)
		{
			IndexBuffer = MoveTemp(Triangles);
		}
		else
		{
			IndexBuffer = Triangles;
		}
	}

	void UpdateTessellationIndexBuffer(TArray<int32>& Triangles, bool bShouldMoveArray)
	{
		if (bShouldMoveArray)
		{
			TessellationIndexBuffer = MoveTemp(Triangles);
		}
		else
		{
			TessellationIndexBuffer = Triangles;
		}
	}

	virtual FRuntimeMeshSectionCreateDataInterface* GetSectionCreationData(FSceneInterface* InScene, UMaterialInterface* InMaterial) const = 0;

	virtual FRuntimeMeshRenderThreadCommandInterface* GetSectionUpdateData(bool bIncludePositionVertices, bool bIncludeVertices, bool bIncludeIndices) const = 0;

	virtual FRuntimeMeshRenderThreadCommandInterface* GetSectionPositionUpdateData() const = 0;

	virtual void RecalculateBoundingBox() = 0;


	virtual int32 GetAllVertexPositions(TArray<FVector>& Positions) = 0;

	virtual void GetInternalVertexComponents(int32& NumUVChannels, bool& WantsHalfPrecisionUVs) { }

	// This is only meant for internal use for supporting the old style create/update sections
	virtual bool UpdateVertexBufferInternal(const TArray<FVector>& Positions, const TArray<FVector>& Normals, const TArray<FRuntimeMeshTangent>& Tangents, const TArray<FVector2D>& UV0, const TArray<FVector2D>& UV1, const TArray<FColor>& Colors) { return false; }
	
	virtual void GetSectionMesh(const IRuntimeMeshVerticesBuilder*& Vertices, const FRuntimeMeshIndicesBuilder*& Indices) = 0;

	virtual const FRuntimeMeshVertexTypeInfo* GetVertexType() const = 0;

	virtual void GenerateNormalTangent() = 0;

	virtual void GenerateTessellationIndices() = 0;


	virtual void Serialize(FArchive& Ar)
	{
		if (Ar.CustomVer(FRuntimeMeshVersion::GUID) >= FRuntimeMeshVersion::DualVertexBuffer)
		{
			Ar << PositionVertexBuffer;
		}

		Ar << IndexBuffer;
		Ar << LocalBoundingBox;
		Ar << CollisionEnabled;
		Ar << bIsVisible;
		int32 UpdateFreq = (int32)UpdateFrequency;
		Ar << UpdateFreq;
		UpdateFrequency = (EUpdateFrequency)UpdateFreq;
	}
	
	friend FArchive& operator <<(FArchive& Ar, FRuntimeMeshSectionInterface& Section)
	{
		Section.Serialize(Ar);
		return Ar;
	}

	friend class FRuntimeMeshSceneProxy;
	friend class URuntimeMeshComponent;
};

namespace RuntimeMeshSectionInternal
{
	template<typename Type>
	static typename TEnableIf<FRuntimeMeshVertexTraits<Type>::HasPosition, int32>::Type
		GetAllVertexPositions(const TArray<Type>& VertexBuffer, const TArray<FVector>& PositionVertexBuffer, TArray<FVector>& Positions)
	{
		int32 VertexCount = VertexBuffer.Num();
		for (int32 VertIdx = 0; VertIdx < VertexCount; VertIdx++)
		{
			Positions.Add(VertexBuffer[VertIdx].Position);
		}
		return VertexCount;
	}

	template<typename Type>
	static typename TEnableIf<!FRuntimeMeshVertexTraits<Type>::HasPosition, int32>::Type
		GetAllVertexPositions(const TArray<Type>& VertexBuffer, const TArray<FVector>& PositionVertexBuffer, TArray<FVector>& Positions)
	{
		Positions.Append(PositionVertexBuffer);
		return PositionVertexBuffer.Num();
	}



	template<typename Type>
	static typename TEnableIf<FRuntimeMeshVertexTraits<Type>::HasPosition, bool>::Type
		UpdateVertexBufferInternal(TArray<Type>& VertexBuffer, FBox& LocalBoundingBox, TArray<Type>& Vertices, const FBox* BoundingBox, bool bShouldMoveArray)
	{
		// Holds the new bounding box after this update.
		FBox NewBoundingBox(0);

		if (bShouldMoveArray)
		{
			// Move buffer data
			VertexBuffer = MoveTemp(Vertices);

			// Calculate the bounding box if one doesn't exist.
			if (BoundingBox == nullptr)
			{
				for (int32 VertexIdx = 0; VertexIdx < VertexBuffer.Num(); VertexIdx++)
				{
					NewBoundingBox += VertexBuffer[VertexIdx].Position;
				}
			}
			else
			{
				// Copy the supplied bounding box instead of calculating it.
				NewBoundingBox = *BoundingBox;
			}
		}
		else
		{
			if (BoundingBox == nullptr)
			{
				// Copy the buffer and calculate the bounding box at the same time
				int32 NumVertices = Vertices.Num();
				VertexBuffer.SetNumUninitialized(NumVertices);
				for (int32 VertexIdx = 0; VertexIdx < NumVertices; VertexIdx++)
				{
					NewBoundingBox += Vertices[VertexIdx].Position;
					VertexBuffer[VertexIdx] = Vertices[VertexIdx];
				}
			}
			else
			{
				// Copy the buffer
				VertexBuffer = Vertices;

				// Copy the supplied bounding box instead of calculating it.
				NewBoundingBox = *BoundingBox;
			}
		}

		// Update the bounding box if necessary and alert our caller if we did
		if (!(LocalBoundingBox == NewBoundingBox))
		{
			LocalBoundingBox = NewBoundingBox;
			return true;
		}

		return false;
	}

	template<typename Type>
	static typename TEnableIf<!FRuntimeMeshVertexTraits<Type>::HasPosition, bool>::Type
		UpdateVertexBufferInternal(TArray<Type>& VertexBuffer, FBox& LocalBoundingBox, TArray<Type>& Vertices, const FBox* BoundingBox, bool bShouldMoveArray)
	{
		if (bShouldMoveArray)
		{
			VertexBuffer = MoveTemp(Vertices);
		}
		else
		{
			VertexBuffer = Vertices;
		}
		return false;
	}


	template<typename Type>
	static typename TEnableIf<FRuntimeMeshVertexTraits<Type>::HasPosition>::Type	RecalculateBoundingBox(TArray<Type>& VertexBuffer, FBox& BoundingBox)
	{
		for (int32 Index = 0; Index < VertexBuffer.Num(); Index++)
		{
			BoundingBox += VertexBuffer[Index].Position;
		}
	}

	template<typename Type>
	static typename TEnableIf<!FRuntimeMeshVertexTraits<Type>::HasPosition>::Type RecalculateBoundingBox(TArray<Type>& VertexBuffer, FBox& BoundingBox)
	{
	}

}

/** Templated class for a single mesh section */
template<typename VertexType>
class FRuntimeMeshSection : public FRuntimeMeshSectionInterface
{

public:
	/** Vertex buffer for this section */
	TArray<VertexType> VertexBuffer;

	FRuntimeMeshSection(bool bInNeedsPositionOnlyBuffer) : FRuntimeMeshSectionInterface(bInNeedsPositionOnlyBuffer) { }
	virtual ~FRuntimeMeshSection() override { }


protected:
	bool UpdateVertexBuffer(TArray<VertexType>& Vertices, const FBox* BoundingBox, bool bShouldMoveArray)
	{
		return RuntimeMeshSectionInternal::UpdateVertexBufferInternal<VertexType>(VertexBuffer, LocalBoundingBox, Vertices, BoundingBox, bShouldMoveArray);
	}

	virtual FRuntimeMeshSectionCreateDataInterface* GetSectionCreationData(FSceneInterface* InScene, UMaterialInterface* InMaterial) const override
	{
		auto UpdateData = new FRuntimeMeshSectionCreateData<VertexType>();

		FMaterialRelevance MaterialRelevance = (InMaterial != nullptr) 
			? InMaterial->GetRelevance(InScene->GetFeatureLevel()) 
			: UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance(InScene->GetFeatureLevel());

		// Create new section proxy based on whether we need separate position buffer
		if (IsDualBufferSection())
		{
			UpdateData->NewProxy = new FRuntimeMeshSectionProxy<VertexType, true>(InScene, UpdateFrequency, bIsVisible, bCastsShadow, InMaterial, MaterialRelevance);
			UpdateData->PositionVertexBuffer = PositionVertexBuffer;
		}
		else
		{
			UpdateData->NewProxy = new FRuntimeMeshSectionProxy<VertexType, false>(InScene, UpdateFrequency, bIsVisible, bCastsShadow, InMaterial, MaterialRelevance);
		}
		const_cast<FRuntimeMeshSection*>(this)->bShouldUseAdjacencyIndexBuffer = UpdateData->NewProxy->ShouldUseAdjacencyIndexBuffer();

		UpdateData->VertexBuffer = VertexBuffer;

		// Switch between normal/tessellation indices

		if (bShouldUseAdjacencyIndexBuffer && TessellationIndexBuffer.Num() > 0)
		{
			UpdateData->IndexBuffer = TessellationIndexBuffer;
			UpdateData->bIsAdjacencyIndexBuffer = true;
		}
		else
		{
			UpdateData->IndexBuffer = IndexBuffer;
			UpdateData->bIsAdjacencyIndexBuffer = false;
		}

		return UpdateData;
	}

	virtual FRuntimeMeshRenderThreadCommandInterface* GetSectionUpdateData(bool bIncludePositionVertices, bool bIncludeVertices, bool bIncludeIndices) const override
	{
		auto UpdateData = new FRuntimeMeshSectionUpdateData<VertexType>();
		UpdateData->bIncludeVertexBuffer = bIncludeVertices;
		UpdateData->bIncludePositionBuffer = bIncludePositionVertices;
		UpdateData->bIncludeIndices = bIncludeIndices;

		if (bIncludePositionVertices)
		{
			UpdateData->PositionVertexBuffer = PositionVertexBuffer;
		}

		if (bIncludeVertices)
		{
			UpdateData->VertexBuffer = VertexBuffer;
		}

		if (bIncludeIndices)
		{
			if (bShouldUseAdjacencyIndexBuffer && TessellationIndexBuffer.Num() > 0)
			{
				UpdateData->IndexBuffer = TessellationIndexBuffer;
				UpdateData->bIsAdjacencyIndexBuffer = true;
			}
			else
			{
				UpdateData->IndexBuffer = IndexBuffer;
				UpdateData->bIsAdjacencyIndexBuffer = false;
			}
		}

		return UpdateData;
	}

	virtual FRuntimeMeshRenderThreadCommandInterface* GetSectionPositionUpdateData() const override
	{
		auto UpdateData = new FRuntimeMeshSectionPositionOnlyUpdateData<VertexType>();

		UpdateData->PositionVertexBuffer = PositionVertexBuffer;

		return UpdateData;
	}

	virtual int32 GetAllVertexPositions(TArray<FVector>& Positions) override
	{
		return RuntimeMeshSectionInternal::GetAllVertexPositions<VertexType>(VertexBuffer, PositionVertexBuffer, Positions);
	}

	virtual void GetSectionMesh(const IRuntimeMeshVerticesBuilder*& Vertices, const FRuntimeMeshIndicesBuilder*& Indices) override
	{
		Vertices = new FRuntimeMeshPackedVerticesBuilder<VertexType>(&VertexBuffer);
		Indices = new FRuntimeMeshIndicesBuilder(&IndexBuffer);
	}

	virtual const FRuntimeMeshVertexTypeInfo* GetVertexType() const { return &VertexType::TypeInfo; }

	virtual void GenerateNormalTangent()
	{
		if (IsDualBufferSection())
		{
			URuntimeMeshLibrary::CalculateTangentsForMesh<VertexType>(PositionVertexBuffer, VertexBuffer, IndexBuffer);
		}
		else
		{
			URuntimeMeshLibrary::CalculateTangentsForMesh<VertexType>(VertexBuffer, IndexBuffer);
		}
	}

	virtual void GenerateTessellationIndices()
	{
		TArray<int32> TessellationIndices;
		if (IsDualBufferSection())
		{
			URuntimeMeshLibrary::GenerateTessellationIndexBuffer<VertexType>(PositionVertexBuffer, VertexBuffer, IndexBuffer, TessellationIndices);
		}
		else
		{
			URuntimeMeshLibrary::GenerateTessellationIndexBuffer<VertexType>(VertexBuffer, IndexBuffer, TessellationIndices);
		}
		UpdateTessellationIndexBuffer(TessellationIndices, true);
	}

	virtual void RecalculateBoundingBox() override
	{
		LocalBoundingBox.Init();

		if (IsDualBufferSection())
		{
			for (int32 Index = 0; Index < PositionVertexBuffer.Num(); Index++)
			{
				LocalBoundingBox += PositionVertexBuffer[Index];
			}
		}
		else
		{
			RuntimeMeshSectionInternal::RecalculateBoundingBox<VertexType>(VertexBuffer, LocalBoundingBox);
		}
	}


	friend class URuntimeMeshComponent;
};


/** Smart pointer to a Runtime Mesh Section */
using RuntimeMeshSectionPtr = TSharedPtr<FRuntimeMeshSectionInterface>;
