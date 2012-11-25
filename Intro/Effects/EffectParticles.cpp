#include "../../GodComplex.h"
#include "EffectParticles.h"

#define CHECK_MATERIAL( pMaterial, ErrorCode )		if ( (pMaterial)->HasErrors() ) m_ErrorCode = ErrorCode;

EffectParticles::EffectParticles() : m_ErrorCode( 0 )
{
	//////////////////////////////////////////////////////////////////////////
	// Create the materials
	CHECK_MATERIAL( m_pMatCompute = CreateMaterial( IDR_SHADER_PARTICLES_COMPUTE, VertexFormatPt4::DESCRIPTOR, "VS", NULL, "PS" ), 1 );
	CHECK_MATERIAL( m_pMatDisplay = CreateMaterial( IDR_SHADER_PARTICLES_DISPLAY, VertexFormatPt4::DESCRIPTOR, "VS", "GS", "PS" ), 2 );
	CHECK_MATERIAL( m_pMatDebugVoronoi = CreateMaterial( IDR_SHADER_PARTICLES_DISPLAY, VertexFormatPt4::DESCRIPTOR, "VS_DEBUG", NULL, "PS_DEBUG" ), 3 );


	//////////////////////////////////////////////////////////////////////////
	// Build the awesome particle primitive
// 	{
// 		VertexFormatP3	Vertices;
// 		m_pPrimParticle = new Primitive( gs_Device, 1, &Vertices, 0, NULL, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, VertexFormatP3::DESCRIPTOR );
// 	}

	//////////////////////////////////////////////////////////////////////////
	// Build our vorono� texture & our initial positions & data
	NjFloat2	pCellCenters[EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT];
	{
		TextureBuilder		TB( 1024, 1024 );
		VertexFormatPt4		pVertices[EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT];
		BuildVoronoiTexture( TB, pCellCenters, pVertices );

		TextureBuilder::ConversionParams	Conv =
		{
			0,		// int		PosR;
			1,		// int		PosG;
			-1,		// int		PosB;
			-1,		// int		PosA;

					// Position of the height & roughness fields
			-1,		// int		PosHeight;
			-1,		// int		PosRoughness;

					// Position of the Material ID
			-1,		// int		PosMatID;

					// Position of the normal fields
			1.0f,	// float	NormalFactor;	// Factor to apply to the height to generate the normals
			-1,		// int		PosNormalX;
			-1,		// int		PosNormalY;
			-1,		// int		PosNormalZ;

					// Position of the AO field
			1.0f,	// float	AOFactor;		// Factor to apply to the height to generate the AO
			-1,		// int		PosAO;
		};

		m_pTexVoronoi = TB.CreateTexture( PixelFormatRG16F::DESCRIPTOR, Conv );

		m_pPrimParticle = new Primitive( gs_Device, EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT, pVertices, 0, NULL, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, VertexFormatPt4::DESCRIPTOR );
	}

	//////////////////////////////////////////////////////////////////////////
	// Build the initial positions & orientations of the particles from the surface of a torus
	PixelFormatRGBA32F*	pInitialPositions = new PixelFormatRGBA32F[EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT];
	PixelFormatRGBA32F*	pInitialNormals = new PixelFormatRGBA32F[EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT];
	PixelFormatRGBA32F*	pInitialTangents = new PixelFormatRGBA32F[EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT];
	PixelFormatRGBA32F*	pScanlinePosition = pInitialPositions;
	PixelFormatRGBA32F*	pScanlineNormal = pInitialNormals;
	PixelFormatRGBA32F*	pScanlineTangent = pInitialTangents;

	int		TotalCount = EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT;
	int		ParticlesPerDimension = U32(floor(powf( float(TotalCount), 1.0f/3.0f )));
	float	R = 0.5f;	// Great radius of the torus
	float	r = 0.2f;	// Small radius of the torus

	for ( int Y=0; Y < EFFECT_PARTICLES_COUNT; Y++ )
		for ( int X=0; X < EFFECT_PARTICLES_COUNT; X++, pScanlinePosition++, pScanlineNormal++, pScanlineTangent++ )
		{
			NjFloat2&	CellCenter = pCellCenters[EFFECT_PARTICLES_COUNT*Y+X];
			float		Alpha = TWOPI * X / EFFECT_PARTICLES_COUNT;	// Angle on the great circle
			float		Beta = TWOPI * Y / EFFECT_PARTICLES_COUNT;	// Angle on the small circle

			NjFloat3	T( cosf(Alpha), 0.0f, -sinf(Alpha) );		// Gives the direction of the center on the great circle
			NjFloat3	Center = NjFloat3( 0, 0.5f, 0 ) + R * T;	// Center on the great circle
			NjFloat3	Ortho( T.z, 0, -T.x );						// Tangent to the great circle
//			NjFloat3	B = T ^ Ortho;
			NjFloat3	B( 0, 1, 0 );								// Obviously, always the UP vector

			NjFloat3	Normal = cosf(Beta) * T + sinf(Beta) * B;	// The normal to the small circle, also the direction to the point on the surface
			NjFloat3	Tangent = Ortho;
			NjFloat3	Pos = Center + r * Normal;					// Position on the surface of the small circle


// DEBUG Generate on a plane for verification
Pos.x = 0.1f * (CellCenter.x - 0.5f * EFFECT_PARTICLES_COUNT);
Pos.y = 0.8;
Pos.z = 0.1f * (CellCenter.y - 0.5f * EFFECT_PARTICLES_COUNT);

Normal.Set( 0, 1, 0 );	// Facing up
Tangent.Set( 1, 0, 0 );	// Right
// DEBUG


			pScanlinePosition->R = Pos.x;
			pScanlinePosition->G = Pos.y;
			pScanlinePosition->B = Pos.z;
			pScanlinePosition->A = 0.0f;

			pScanlineNormal->R = Normal.x;
			pScanlineNormal->G = Normal.y;
			pScanlineNormal->B = Normal.z;
			pScanlineNormal->A = 0.0f;

			pScanlineTangent->R = Tangent.x;
			pScanlineTangent->G = Tangent.y;
			pScanlineTangent->B = Tangent.z;
			pScanlineTangent->A = 0.0f;
		}

	// Unfortunately, we need to create Textures to initialize our RenderTargets
	void*		ppContentPositions[1];
				ppContentPositions[0] = (void*) pInitialPositions;
	Texture2D*	pTempPositions = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, ppContentPositions );
	delete[]	pInitialPositions;

	void*		ppContentNormals[1];
				ppContentNormals[0] = (void*) pInitialNormals;
	Texture2D*	pTempNormals = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, ppContentNormals );
	delete[]	pInitialNormals;

	void*		ppContentTangents[1];
				ppContentTangents[0] = (void*) pInitialTangents;
	Texture2D*	pTempTangents = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, ppContentTangents );
	delete[]	pInitialTangents;

	// Finally, create the render targets and initialize them
	m_ppRTParticlePositions[0] = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, NULL );
	m_ppRTParticlePositions[1] = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, NULL );
	m_ppRTParticlePositions[2] = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, NULL );
	m_ppRTParticlePositions[0]->CopyFrom( *pTempPositions );
	m_ppRTParticlePositions[1]->CopyFrom( *pTempPositions );
	m_ppRTParticlePositions[2]->CopyFrom( *pTempPositions );
	delete	pTempPositions;

	m_ppRTParticleNormals[0] = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, NULL );
	m_ppRTParticleNormals[1] = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, NULL );
	m_ppRTParticleNormals[0]->CopyFrom( *pTempNormals );
	m_ppRTParticleNormals[1]->CopyFrom( *pTempNormals );
	delete pTempNormals;

	m_ppRTParticleTangents[0] = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, NULL );
	m_ppRTParticleTangents[1] = new Texture2D( gs_Device, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, 1, PixelFormatRGBA32F::DESCRIPTOR, 1, NULL );
	m_ppRTParticleTangents[0]->CopyFrom( *pTempTangents );
	m_ppRTParticleTangents[1]->CopyFrom( *pTempTangents );
	delete pTempTangents;


	//////////////////////////////////////////////////////////////////////////
	// Create the constant buffers
	m_pCB_Render = new CB<CBRender>( gs_Device, 10 );
	m_pCB_Render->m.DeltaTime.Set( 0, 1 );
}

EffectParticles::~EffectParticles()
{
	delete m_pTexVoronoi;

	delete m_pCB_Render;

	delete m_ppRTParticleTangents[0];
	delete m_ppRTParticleTangents[1];
	delete m_ppRTParticleNormals[0];
	delete m_ppRTParticleNormals[1];
	delete m_ppRTParticlePositions[2];
	delete m_ppRTParticlePositions[1];
	delete m_ppRTParticlePositions[0];

	delete m_pPrimParticle;

	delete m_pMatCompute;
 	delete m_pMatDisplay;
	delete m_pMatDebugVoronoi;
}

void	EffectParticles::Render( float _Time, float _DeltaTime )
{
	//////////////////////////////////////////////////////////////////////////
	// 1] Update particles' positions
	{	USING_MATERIAL_START( *m_pMatCompute )
	
		ID3D11RenderTargetView*	ppRenderTargets[3] =
		{
			m_ppRTParticlePositions[2]->GetTargetView( 0, 0, 1 ), m_ppRTParticleNormals[1]->GetTargetView( 0, 0, 1 ), m_ppRTParticleTangents[1]->GetTargetView( 0, 0, 1 )
		};
		gs_Device.SetRenderTargets( m_ppRTParticlePositions[2]->GetWidth(), m_ppRTParticlePositions[2]->GetHeight(), 3, ppRenderTargets );
 		gs_Device.SetStates( gs_Device.m_pRS_CullNone, gs_Device.m_pDS_Disabled, gs_Device.m_pBS_Disabled );

		m_pCB_Render->m.dUV = m_ppRTParticlePositions[2]->GetdUV();
		m_pCB_Render->m.DeltaTime.x = 10.0f * _DeltaTime;
		m_pCB_Render->UpdateData();

		m_ppRTParticlePositions[0]->SetPS( 10 );
		m_ppRTParticlePositions[1]->SetPS( 11 );
		m_ppRTParticleNormals[0]->SetPS( 12 );
		m_ppRTParticleTangents[0]->SetPS( 13 );

		gs_pPrimQuad->Render( *m_pMatCompute );

		// Scroll positions for integration next frame
		Texture2D*	pTemp = m_ppRTParticlePositions[0];
		m_ppRTParticlePositions[0] = m_ppRTParticlePositions[1];
		m_ppRTParticlePositions[1] = m_ppRTParticlePositions[2];
		m_ppRTParticlePositions[2] = pTemp;

		// Swap normals & tangents
		pTemp = m_ppRTParticleNormals[0];
		m_ppRTParticleNormals[0] = m_ppRTParticleNormals[1];
		m_ppRTParticleNormals[1] = pTemp;

		pTemp = m_ppRTParticleTangents[0];
		m_ppRTParticleTangents[0] = m_ppRTParticleTangents[1];
		m_ppRTParticleTangents[1] = pTemp;

		// Keep delta time for next time
		m_pCB_Render->m.DeltaTime.y = 1.0f;//_DeltaTime;

		USING_MATERIAL_END
	}

	//////////////////////////////////////////////////////////////////////////
	// 2] Render the particles
	{	USING_MATERIAL_START( *m_pMatDisplay )

		gs_Device.SetRenderTarget( gs_Device.DefaultRenderTarget(), &gs_Device.DefaultDepthStencil() );
		gs_Device.SetStates( gs_Device.m_pRS_CullNone, gs_Device.m_pDS_ReadWriteLess, gs_Device.m_pBS_Disabled );

		m_ppRTParticlePositions[1]->SetVS( 10 );
		m_ppRTParticleNormals[0]->SetVS( 11 );
		m_ppRTParticleTangents[0]->SetVS( 12 );
		m_pTexVoronoi->SetPS( 13 );

//		m_pPrimParticle->RenderInstanced( *m_pMatDisplay, EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT );
		m_pPrimParticle->Render( *m_pMatDisplay );

		USING_MATERIAL_END
	}

// DEBUG
{	USING_MATERIAL_START( *m_pMatDebugVoronoi )

	D3D11_VIEWPORT	Vp = 
{
0, // FLOAT TopLeftX;
0, // FLOAT TopLeftY;
0.2f * RESX, // FLOAT Width;
0.2f * RESY, // FLOAT Height;
0, // FLOAT MinDepth;
1, // FLOAT MaxDepth;
};
	gs_Device.SetRenderTarget( gs_Device.DefaultRenderTarget(), &gs_Device.DefaultDepthStencil(), &Vp );
	gs_Device.SetStates( gs_Device.m_pRS_CullNone, gs_Device.m_pDS_Disabled, gs_Device.m_pBS_Disabled );

	m_pTexVoronoi->SetPS( 10 );

	gs_pPrimQuad->Render( *m_pMatDebugVoronoi );

	USING_MATERIAL_END
}
// DEBUG
}

namespace	// Drawers & Fillers
{
	struct	__VoronoiInfos
	{
		int					ParticleIndex;
		float				Distance;
	};
	struct	__PerturbVoronoi
	{
		Noise*				pNoise;
		TextureBuilder*		pVoronoi;
		int					W, H;
		VertexFormatPt4*	pVertices;
	};

	float	CombineDistances( float _pDistances[], int _pCellX[], int _pCellY[], int _pCellZ[], void* _pData )
	{
		__VoronoiInfos&	Infos = *((__VoronoiInfos*) _pData);
		Infos.ParticleIndex = EFFECT_PARTICLES_COUNT*_pCellY[0] + _pCellX[0];
		Infos.Distance = sqrtf( _pDistances[0] );
		return 0.0f;
	}
	void	FillVoronoi( int x, int y, const NjFloat2& _UV, Pixel& _Pixel, void* _pData )
	{
 		Noise&	N = *((Noise*) _pData);

		__VoronoiInfos	Infos;
 		N.Cellular( EFFECT_PARTICLES_COUNT * _UV, CombineDistances, &Infos, true );	// Simple cellular (NOT Worley !) => Means only 1 point per cell, exactly what we need for a single particle ID

		_Pixel.RGBA.Set( float(Infos.ParticleIndex), Infos.Distance, 0, 0 );
	}
	void	PerturbVoronoi( int x, int y, const NjFloat2& _UV, Pixel& _Pixel, void* _pData )
	{
 		__PerturbVoronoi&	Params = *((__PerturbVoronoi*) _pData);
		
		// Perturb the UVs a little
		NjFloat2	Disturb = Params.pNoise->PerlinVector( 0.025f * _UV );
		NjFloat2	NewUV = _UV + 0*0.04f * Disturb;

		// Use POINT SAMPLING to fetch the original color from the vorono� texture
		// (because we're dealing with particle indices here, not colors that can be linearly interpolated!)
		int		X = (Params.W + int(floorf( NewUV.x * Params.W ))) % Params.W;
		int		Y = (Params.H + int(floorf( NewUV.y * Params.H ))) % Params.H;
		Params.pVoronoi->Get( X, Y, 0, _Pixel );

		// Update the particles's Min/Max UVs
		int			ParticleIndex = int(_Pixel.RGBA.x);
		ASSERT(ParticleIndex >= 0, "WTF?!" );
		ASSERT(ParticleIndex < EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT, "WTF?!" );
		NjFloat4&	ParticleVertex = Params.pVertices[ParticleIndex].Pt;

		if ( ParticleIndex == 0x2F && NewUV.x < 0.4f )
			ParticleIndex++;

		// Check if new UV is not too far from existing min/max boundaries, in which case it would indicate a wrap
		// We want to overlap the border instead...
		float	DeltaU = NewUV.x - ParticleVertex.x;
		float	DeltaV = NewUV.y - ParticleVertex.y;
		if ( DeltaU > 0.2f )
			NewUV.x -= 1.0f;	// Overlap instead
		if ( DeltaV > 0.2f )
			NewUV.y -= 1.0f;	// Overlap instead

		DeltaU = ParticleVertex.z - NewUV.x;
		DeltaV = ParticleVertex.w - NewUV.y;
		if ( DeltaU > 0.2f )
			NewUV.x += 1.0f;	// Overlap instead
		if ( DeltaV > 0.2f )
			NewUV.y += 1.0f;	// Overlap instead

		ParticleVertex.x = MIN( ParticleVertex.x, NewUV.x );
		ParticleVertex.y = MIN( ParticleVertex.y, NewUV.y );
		ParticleVertex.z = MAX( ParticleVertex.z, NewUV.x );
		ParticleVertex.w = MAX( ParticleVertex.w, NewUV.y );
	}
};

void	EffectParticles::BuildVoronoiTexture( TextureBuilder& _TB, NjFloat2* _pCellCenters, VertexFormatPt4* _pVertices )
{
	// Build a vorono� pattern
	Noise	N( 1 );
			N.SetCellularWrappingParameters( EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT, EFFECT_PARTICLES_COUNT );

	TextureBuilder	TempVoronoi( _TB.GetWidth(), _TB.GetHeight() );
					TempVoronoi.Fill( ::FillVoronoi, &N );

	// Clear the vertices to wrong intervals
	for ( int ParticleIndex=0; ParticleIndex < EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT; ParticleIndex++ )
		_pVertices[ParticleIndex].Pt.Set( +MAX_FLOAT, +MAX_FLOAT, -MAX_FLOAT, -MAX_FLOAT );

	// Perturb the original vorono� with a small noise to break the regular cell patterns
	__PerturbVoronoi	S = { &N, &TempVoronoi, TempVoronoi.GetWidth(), TempVoronoi.GetHeight(), _pVertices };
	_TB.Fill( ::PerturbVoronoi, &S );

	// Reparse vertices to make sure border particles wrap correctly
// 	for ( int VertexIndex=0; VertexIndex < EFFECT_PARTICLES_COUNT*EFFECT_PARTICLES_COUNT; VertexIndex++ )
// 	{
// 		NjFloat4&	UVs = _pVertices[VertexIndex].Pt;
// 
// 		float		DeltaU = UVs.z - UVs.x;
// 		float		DeltaV = UVs.w - UVs.y;
// 
// 		ASSERT( DeltaU < 0.2f, "WTF?!" );
// 		ASSERT( DeltaV < 0.2f, "WTF?!" );
//  	}

	// Generate the positions of the center of each cell (in UV space)
	for ( int Y=0; Y < EFFECT_PARTICLES_COUNT; Y++ )
		for ( int X=0; X < EFFECT_PARTICLES_COUNT; X++ )
			N.CellularGetCenter( X, Y, _pCellCenters[EFFECT_PARTICLES_COUNT*Y+X], true );
}