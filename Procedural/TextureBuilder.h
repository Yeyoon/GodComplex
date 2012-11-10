//////////////////////////////////////////////////////////////////////////
// Helps to build a texture and its mip levels to provide a valid buffer when constructing a Texture2D
// Note that the resulting texture is always a Texture2DArray where the various fields are populated as dictated by the ConversionParams structure
//
#pragma once

#include "FatPixel.h"

class	TextureBuilder
{
protected:	// CONSTANTS

public:		// NESTED TYPES

	typedef void	(*FillDelegate)( int _X, int _Y, const NjFloat2& _UV, Pixel& _Pixel, void* _pData );

	// The complex structure that is guiding the texture conversion
	// Use -1 in field positions to avoid storing the field
	// * If you use only [1,4] fields, a single texture will be generated
	// * If you use only [5,8] fields, 2 textures will be generated
	// * If you use only [9,12] fields, 3 textures will be generated
	//	etc.
	//
	// Check the existing presets for typical cases.
	struct	ConversionParams
	{
		// Positions of the color fields
		int		PosR;
		int		PosG;
		int		PosB;
		int		PosA;

		// Position of the height & roughness fields
		int		PosHeight;
		int		PosRoughness;

		// Position of the MaterialID
		int		PosMatID;

		// Position of the normal fields
		float	NormalFactor;	// Factor to apply to the height to generate the normals
		int		PosNormalX;		// As soon as one of these positions is different of -1, normal will be generated
		int		PosNormalY;
		int		PosNormalZ;		// If -1, normal will get normalized and packed only as XY. Z will then be extracted by sqrt(1-X�-Y�)

		// Position of the AO field
		float	AOFactor;		// Factor to apply to the height to generate the AO
		int		PosAO;			// If not -1, AO will be generated

		// TODO: Curvature? Dirt accumulation? Gradient?
	};

	static ConversionParams		CONV_RGBA_NxNyHR_M;	// Generates an array of 3 textures: 1st is RGBA, 2nd is Normal(X+Y), Height, Roughness, 3rd is MaterialID
	static ConversionParams		CONV_NxNyNzH;		// Generates an array of 1 texture: Normal(X+Y+Z) + Height


protected:	// FIELDS

	int				m_Width;
	int				m_Height;
	int				m_MipLevelsCount;
	mutable bool	m_bMipLevelsBuilt;

	Pixel**			m_ppBufferGeneric;		// Generic buffer consisting of meta-pixels
	int*			m_pMipSizes;
	mutable void**	m_ppBufferSpecific;		// Specific buffer of given pixel format


public:		// PROPERTIES

	int				GetWidth() const	{ return m_Width; }
	int				GetHeight() const	{ return m_Height; }
	int				GetWidth( int _MipLevel ) const	{ return m_pMipSizes[(_MipLevel<<1)+0]; }
	int				GetHeight( int _MipLevel ) const	{ return m_pMipSizes[(_MipLevel<<1)+1]; }

	Pixel**			GetMips()			{ return m_ppBufferGeneric; }
	const void**	GetLastConvertedMips() const;


public:		// METHODS

	TextureBuilder( int _Width, int _Height );
 	~TextureBuilder();

	void			CopyFromFast( const TextureBuilder& _Source );	// Copies from a source TB using mip 0 only
	void			CopyFrom( const TextureBuilder& _Source );		// Same but if the sizes are different and target is smaller, the copy will be performed using the best mip level as source (implies generation of the mip maps on the source builder)
	void			Clear( const Pixel& _Pixel );
	void			Fill( FillDelegate _Filler, void* _pData );
	void			Get( int _X, int _Y, int _MipLevel, Pixel& _Color ) const;
	void			SampleWrap( float _X, float _Y, int _MipLevel, Pixel& _Pixel ) const;
	void			SampleClamp( float _X, float _Y, int _MipLevel, Pixel& _Pixel ) const;
	void			GenerateMips( bool _bTreatRGBAsNormal=false ) const;

	// Converts the generic content into an array of mip-maps of a specific pixel format, ready to build a Texture2D
	// NOTE: You don't need to delete the returned pointers
	void**			Convert( const IPixelFormatDescriptor& _Format, const ConversionParams& _Params, int& _ArraySize ) const;

	// Call Convert() and directly generate a texture
	Texture2D*		CreateTexture( const IPixelFormatDescriptor& _Format, const ConversionParams& _Params, bool _bStaging=false, bool _bWriteable=false ) const;

private:
	void			ReleaseSpecificBuffer() const;
	float			BuildComponent( int _ComponentIndex, const ConversionParams& _Params, Pixel& _Pixel0, Pixel& _Pixel1, Pixel& _Pixel2 ) const;
};
