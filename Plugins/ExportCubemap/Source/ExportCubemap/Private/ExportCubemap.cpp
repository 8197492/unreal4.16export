// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExportCubemap.h"
#include "ExportCubemapStyle.h"
#include "ExportCubemapCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"
#include "Engine/Selection.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Engine/SphereReflectionCapture.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "Components/ReflectionCaptureComponent.h"
#include <fstream>
static const FName ExportCubemapTabName("ExportCubemap");

#define LOCTEXT_NAMESPACE "FExportCubemapModule"


void FExportCubemapModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FExportCubemapStyle::Initialize();
	FExportCubemapStyle::ReloadTextures();

	FExportCubemapCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FExportCubemapCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FExportCubemapModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FExportCubemapModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FExportCubemapModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
}

void FExportCubemapModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FExportCubemapStyle::Shutdown();

	FExportCubemapCommands::Unregister();
}

#define DDSD_CAPS  0x1
#define DDSD_HEIGHT  0x2
#define DDSD_WIDTH  0x4
#define DDSD_PITCH  0x8
#define DDSD_PIXELFORMAT  0x1000
#define DDSD_MIPMAPCOUNT  0x20000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_DEPTH 0x800000

#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_ALPHA       0x00000002
#define DDPF_FOURCC 0x00000004
#define DDPF_RGB    0x00000040
#define DDSF_RGBA  0x00000041
#define DDPF_YUV 0x200
#define DDPF_LUMINANCE 0x20000

#define DDSCAPS_COMPLEX 0x8
#define DDSCAPS_MIPMAP 0x400000
#define DDSCAPS_TEXTURE 0x1000

#define DDSCAPS2_CUBEMAP 0x200
#define DDSCAPS2_CUBEMAP_POSITIVEX 0x400
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x800
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x1000
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x2000
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x4000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x8000
#define DDSCAPS2_CUBEMAP_ALL_FACES 0x0000FC00
#define DDSCAPS2_VOLUME 0x200000

// dwCaps2 flags
const uint32 DDSF_CUBEMAP = 0x00000200;
const uint32 DDSF_CUBEMAP_POSITIVEX = 0x00000400;
const uint32 DDSF_CUBEMAP_NEGATIVEX = 0x00000800;
const uint32 DDSF_CUBEMAP_POSITIVEY = 0x00001000;
const uint32 DDSF_CUBEMAP_NEGATIVEY = 0x00002000;
const uint32 DDSF_CUBEMAP_POSITIVEZ = 0x00004000;
const uint32 DDSF_CUBEMAP_NEGATIVEZ = 0x00008000;
const uint32 DDSF_CUBEMAP_ALL_FACES = 0x0000FC00;
const uint32 DDSF_VOLUME = 0x00200000;

#define GL_BGR_EXT                                        0x80E0
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                  0x83F3

#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_LUMINANCE                      0x1909
#define GL_BGR_EXT                        0x80E0
#define GL_BGRA_EXT                       0x80E1

// compressed texture types
const uint32_t FOURCC_DXT1 = 0x31545844; //(MAKEFOURCC('D','X','T','1'))
const uint32_t FOURCC_DXT3 = 0x33545844; //(MAKEFOURCC('D','X','T','3'))
const uint32_t FOURCC_DXT5 = 0x35545844; //(MAKEFOURCC('D','X','T','5'))

FString fourcc(uint32_t enc) {
	char c[5] = { '\0' };
	c[0] = enc >> 0 & 0xFF;
	c[1] = enc >> 8 & 0xFF;
	c[2] = enc >> 16 & 0xFF;
	c[3] = enc >> 24 & 0xFF;
	return c;
}

struct DXTColBlock {
	uint16_t col0;
	uint16_t col1;

	uint8_t row[4];
};

struct DXT3AlphaBlock {
	uint16_t row[4];
};

struct DXT5AlphaBlock {
	uint8_t alpha0;
	uint8_t alpha1;

	uint8_t row[6];
};

///////////////////////////////////////////////////////////////////////////////
// flip a DXT1 color block
void flip_blocks_dxtc1(DXTColBlock *line, unsigned int numBlocks) {
	DXTColBlock *curblock = line;

	for (unsigned int i = 0; i < numBlocks; i++) {
		std::swap(curblock->row[0], curblock->row[3]);
		std::swap(curblock->row[1], curblock->row[2]);

		curblock++;
	}
}

///////////////////////////////////////////////////////////////////////////////
// flip a DXT3 color block
void flip_blocks_dxtc3(DXTColBlock *line, unsigned int numBlocks) {
	DXTColBlock *curblock = line;
	DXT3AlphaBlock *alphablock;

	for (unsigned int i = 0; i < numBlocks; i++) {
		alphablock = (DXT3AlphaBlock*)curblock;

		std::swap(alphablock->row[0], alphablock->row[3]);
		std::swap(alphablock->row[1], alphablock->row[2]);

		curblock++;

		std::swap(curblock->row[0], curblock->row[3]);
		std::swap(curblock->row[1], curblock->row[2]);

		curblock++;
	}
}

///////////////////////////////////////////////////////////////////////////////
// flip a DXT5 alpha block
void flip_dxt5_alpha(DXT5AlphaBlock *block) {
	uint8_t gBits[4][4];

	const uint32_t mask = 0x00000007;          // bits = 00 00 01 11
	uint32_t bits = 0;
	memcpy(&bits, &block->row[0], sizeof(uint8_t) * 3);

	gBits[0][0] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[0][1] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[0][2] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[0][3] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[1][0] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[1][1] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[1][2] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[1][3] = (uint8_t)(bits & mask);

	bits = 0;
	memcpy(&bits, &block->row[3], sizeof(uint8_t) * 3);

	gBits[2][0] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[2][1] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[2][2] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[2][3] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[3][0] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[3][1] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[3][2] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[3][3] = (uint8_t)(bits & mask);

	uint32_t *pBits = ((uint32_t*) &(block->row[0]));

	*pBits = *pBits | (gBits[3][0] << 0);
	*pBits = *pBits | (gBits[3][1] << 3);
	*pBits = *pBits | (gBits[3][2] << 6);
	*pBits = *pBits | (gBits[3][3] << 9);

	*pBits = *pBits | (gBits[2][0] << 12);
	*pBits = *pBits | (gBits[2][1] << 15);
	*pBits = *pBits | (gBits[2][2] << 18);
	*pBits = *pBits | (gBits[2][3] << 21);

	pBits = ((uint32_t*) &(block->row[3]));

#ifdef MACOS
	*pBits &= 0x000000ff;
#else
	*pBits &= 0xff000000;
#endif

	*pBits = *pBits | (gBits[1][0] << 0);
	*pBits = *pBits | (gBits[1][1] << 3);
	*pBits = *pBits | (gBits[1][2] << 6);
	*pBits = *pBits | (gBits[1][3] << 9);

	*pBits = *pBits | (gBits[0][0] << 12);
	*pBits = *pBits | (gBits[0][1] << 15);
	*pBits = *pBits | (gBits[0][2] << 18);
	*pBits = *pBits | (gBits[0][3] << 21);
}

///////////////////////////////////////////////////////////////////////////////
// flip a DXT5 color block
void flip_blocks_dxtc5(DXTColBlock *line, unsigned int numBlocks) {
	DXTColBlock *curblock = line;
	DXT5AlphaBlock *alphablock;

	for (unsigned int i = 0; i < numBlocks; i++) {
		alphablock = (DXT5AlphaBlock*)curblock;

		flip_dxt5_alpha(alphablock);

		curblock++;

		std::swap(curblock->row[0], curblock->row[3]);
		std::swap(curblock->row[1], curblock->row[2]);

		curblock++;
	}
}

struct ReflectionInfo
{
	FString m_strName;
	FVector m_v3Position;
	FVector m_v3Offset;
	float m_fBrightness;
	float m_fInfluenceRadius;
	float m_fAverageBrightness;
	const FReflectionCaptureFullHDR* m_pkData = nullptr;
};

typedef struct {
	uint32 dwSize;
	uint32 dwFlags;
	uint32 dwFourCC;
	uint32 dwRGBBitCount;
	uint32 dwRBitMask;
	uint32 dwGBitMask;
	uint32 dwBBitMask;
	uint32 dwABitMask;
} DDS_PIXELFORMAT;

struct DDS_HEADER
{
	uint32           dwSize;
	uint32           dwFlags;
	uint32           dwHeight;
	uint32           dwWidth;
	uint32           dwLinearSize;
	uint32           dwDepth;
	uint32           dwMipMapCount;
	uint32           dwReserved1[11];
	DDS_PIXELFORMAT ddpf;
	uint32           dwCaps;
	uint32           dwCaps2;
	uint32           dwCaps3;
	uint32           dwCaps4;
	uint32           dwReserved2;
};

#define FACE_X_POS 0
#define FACE_X_NEG 1
#define FACE_Y_POS 2
#define FACE_Y_NEG 3
#define FACE_Z_POS 4
#define FACE_Z_NEG 5

#define EDGE_LEFT   0	 // u = 0
#define EDGE_RIGHT  1	 // u = 1
#define EDGE_TOP    2	 // v = 0
#define EDGE_BOTTOM 3	 // v = 1

#define CORNER_NNN  0
#define CORNER_NNP  1
#define CORNER_NPN  2
#define CORNER_NPP  3
#define CORNER_PNN  4
#define CORNER_PNP  5
#define CORNER_PPN  6
#define CORNER_PPP  7

static const int32 CubeEdgeListA[12][2] =
{
	{ FACE_X_POS, EDGE_LEFT },
	{ FACE_X_POS, EDGE_RIGHT },
	{ FACE_X_POS, EDGE_TOP },
	{ FACE_X_POS, EDGE_BOTTOM },

	{ FACE_X_NEG, EDGE_LEFT },
	{ FACE_X_NEG, EDGE_RIGHT },
	{ FACE_X_NEG, EDGE_TOP },
	{ FACE_X_NEG, EDGE_BOTTOM },

	{ FACE_Z_POS, EDGE_TOP },
	{ FACE_Z_POS, EDGE_BOTTOM },
	{ FACE_Z_NEG, EDGE_TOP },
	{ FACE_Z_NEG, EDGE_BOTTOM }
};

static const int32 CubeEdgeListB[12][2] =
{
	{ FACE_Z_POS, EDGE_RIGHT },
	{ FACE_Z_NEG, EDGE_LEFT },
	{ FACE_Y_POS, EDGE_RIGHT },
	{ FACE_Y_NEG, EDGE_RIGHT },

	{ FACE_Z_NEG, EDGE_RIGHT },
	{ FACE_Z_POS, EDGE_LEFT },
	{ FACE_Y_POS, EDGE_LEFT },
	{ FACE_Y_NEG, EDGE_LEFT },

	{ FACE_Y_POS, EDGE_BOTTOM },
	{ FACE_Y_NEG, EDGE_TOP },
	{ FACE_Y_POS, EDGE_TOP },
	{ FACE_Y_NEG, EDGE_BOTTOM },
};

static const int32 CubeCornerList[6][4] =
{
	{ CORNER_PPP, CORNER_PPN, CORNER_PNP, CORNER_PNN },
	{ CORNER_NPN, CORNER_NPP, CORNER_NNN, CORNER_NNP },
	{ CORNER_NPN, CORNER_PPN, CORNER_NPP, CORNER_PPP },
	{ CORNER_NNP, CORNER_PNP, CORNER_NNN, CORNER_PNN },
	{ CORNER_NPP, CORNER_PPP, CORNER_NNP, CORNER_PNP },
	{ CORNER_PPN, CORNER_NPN, CORNER_PNN, CORNER_NNN }
};

enum TextureType {
	TextureNone, TextureFlat,    // 1D, 2D textures
	Texture3D,
	TextureCubemap
};
class CSurface {
public:
	CSurface();
	CSurface(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels);
	CSurface(const CSurface &copy);
	CSurface &operator=(const CSurface &rhs);
	virtual ~CSurface();

	operator uint8*() const;

	virtual void create(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels);
	virtual void clear();

	unsigned int get_width() const {
		return m_width;
	}
	unsigned int get_height() const {
		return m_height;
	}
	unsigned int get_depth() const {
		return m_depth;
	}
	unsigned int get_size() const {
		return m_size;
	}

	friend class CTexture;
	unsigned int m_width;
	unsigned int m_height;
	unsigned int m_depth;
	unsigned int m_size;

	uint8 *m_pixels;
};

//////////////////////////////////////////////////////////////////////////////
// CSurface implementation
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// default constructor
CSurface::CSurface() :
	m_width(0), m_height(0), m_depth(0), m_size(0), m_pixels(NULL) {
}

///////////////////////////////////////////////////////////////////////////////
// creates an empty image
CSurface::CSurface(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels) :
	m_width(0), m_height(0), m_depth(0), m_size(0), m_pixels(NULL) {
	create(w, h, d, imgsize, pixels);
}

///////////////////////////////////////////////////////////////////////////////
// copy constructor
CSurface::CSurface(const CSurface &copy) :
	m_width(0), m_height(0), m_depth(0), m_size(0), m_pixels(NULL) {
	if (copy.get_size() != 0) {
		m_size = copy.get_size();
		m_width = copy.get_width();
		m_height = copy.get_height();
		m_depth = copy.get_depth();

		m_pixels = new uint8_t[m_size];
		memcpy(m_pixels, copy, m_size);
	}
}

///////////////////////////////////////////////////////////////////////////////
// assignment operator
CSurface &CSurface::operator=(const CSurface &rhs) {
	if (this != &rhs) {
		clear();

		if (rhs.get_size()) {
			m_size = rhs.get_size();
			m_width = rhs.get_width();
			m_height = rhs.get_height();
			m_depth = rhs.get_depth();

			m_pixels = new uint8[m_size];
			memcpy(m_pixels, rhs, m_size);
		}
	}

	return *this;
}

///////////////////////////////////////////////////////////////////////////////
// clean up image memory
CSurface::~CSurface() {
	clear();
}

///////////////////////////////////////////////////////////////////////////////
// returns a pointer to image
CSurface::operator uint8_t*() const {
	return m_pixels;
}

///////////////////////////////////////////////////////////////////////////////
// creates an empty image
void CSurface::create(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels) {

	clear();

	m_width = w;
	m_height = h;
	m_depth = d;
	m_size = imgsize;
	m_pixels = new uint8_t[imgsize];
	memcpy(m_pixels, pixels, imgsize);
}

///////////////////////////////////////////////////////////////////////////////
// free surface memory
void CSurface::clear() {
	if (m_pixels != NULL) {
		delete[] m_pixels;
		m_pixels = NULL;
	}
}

class CTexture : public CSurface {
	friend class CDDSImage;

public:
	CTexture();
	CTexture(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8_t *pixels);
	CTexture(const CTexture &copy);
	CTexture &operator=(const CTexture &rhs);
	~CTexture();

	void create(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8_t *pixels);
	void clear();

	const CSurface &get_mipmap(unsigned int index) const {

		return m_mipmaps[index];
	}

	void add_mipmap(const CSurface &mipmap) {
		m_mipmaps.Push(mipmap);
	}

	unsigned int get_num_mipmaps() const {
		return (unsigned int)m_mipmaps.Num();
	}

	void FlipX()
	{
		uint32* buffer = (uint32*)m_pixels;
		for (uint32 i(0); i < m_height; ++i)
		{
			for (uint32 j(0); j < (m_width >> 1); ++j)
			{
				uint32 temp = buffer[i * m_width + j];
				buffer[i * m_width + j] = buffer[i * m_width + m_width - j - 1];
				buffer[i * m_width + m_width - j - 1] = temp;
			}
		}

		for (auto& sur : m_mipmaps)
		{
			buffer = (uint32*)sur.m_pixels;
			for (uint32 i(0); i < sur.m_height; ++i)
			{
				for (uint32 j(0); j < (sur.m_width >> 1); ++j)
				{
					uint32 temp = buffer[i * sur.m_width + j];
					buffer[i * sur.m_width + j] = buffer[i * sur.m_width + sur.m_width - j - 1];
					buffer[i * sur.m_width + sur.m_width - j - 1] = temp;
				}
			}
		}
	}

protected:
	CSurface &get_mipmap(unsigned int index) {

		return m_mipmaps[index];
	}

private:
	TArray<CSurface> m_mipmaps;
};

///////////////////////////////////////////////////////////////////////////////
// CTexture implementation
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// default constructor
CTexture::CTexture() :
	CSurface()  // initialize base class part
{
}

///////////////////////////////////////////////////////////////////////////////
// creates an empty texture
CTexture::CTexture(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels) :
	CSurface(w, h, d, imgsize, pixels)  // initialize base class part
{
}

CTexture::~CTexture() {
}

///////////////////////////////////////////////////////////////////////////////
// copy constructor
CTexture::CTexture(const CTexture &copy) :
	CSurface(copy) {
	for (unsigned int i = 0; i < copy.get_num_mipmaps(); i++)
		m_mipmaps.Push(copy.get_mipmap(i));
}

///////////////////////////////////////////////////////////////////////////////
// assignment operator
CTexture &CTexture::operator=(const CTexture &rhs) {
	if (this != &rhs) {
		CSurface::operator =(rhs);

		m_mipmaps.Empty();
		for (unsigned int i = 0; i < rhs.get_num_mipmaps(); i++)
			m_mipmaps.Push(rhs.get_mipmap(i));
	}

	return *this;
}

void CTexture::create(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels) {
	CSurface::create(w, h, d, imgsize, pixels);

	m_mipmaps.Empty();
}

void CTexture::clear() {
	CSurface::clear();

	m_mipmaps.Empty();
}


class CDDSImage {
public:
	CDDSImage();
	~CDDSImage();

	void create_textureFlat(unsigned int format, unsigned int components, const CTexture &baseImage);
	void create_texture3D(unsigned int format, unsigned int components, const CTexture &baseImage);
	void create_textureCubemap(unsigned int format, unsigned int components, const CTexture &positiveX, const CTexture &negativeX, const CTexture &positiveY,
		const CTexture &negativeY, const CTexture &positiveZ, const CTexture &negativeZ);

	void clear();

	void load(std::istream& is, bool flipImage = true);
	void load(const FString& filename, bool flipImage = true);
	void save(const FString& filename, bool flipImage = true);


	operator uint8_t*() {

		return m_images[0];
	}

	unsigned int get_width() {
		return m_images[0].get_width();
	}

	unsigned int get_height() {
		return m_images[0].get_height();
	}

	unsigned int get_depth() {
		return m_images[0].get_depth();
	}

	unsigned int get_size() {
		return m_images[0].get_size();
	}

	unsigned int get_num_mipmaps() {
		return m_images[0].get_num_mipmaps();
	}

	const CSurface &get_mipmap(unsigned int index) const {
		return m_images[0].get_mipmap(index);
	}

	const CTexture &get_cubemap_face(unsigned int face) const {
		return m_images[face];
	}

	unsigned int get_components() {
		return m_components;
	}
	unsigned int get_format() {
		return m_format;
	}
	TextureType get_type() {
		return m_type;
	}

	bool is_compressed();

	bool is_cubemap() {
		return (m_type == TextureCubemap);
	}
	bool is_volume() {
		return (m_type == Texture3D);
	}
	bool is_valid() {
		return m_valid;
	}

	bool is_dword_aligned() {

		int dwordLineSize = get_dword_aligned_linesize(get_width(), m_components * 8);
		int curLineSize = get_width() * m_components;

		return (dwordLineSize == curLineSize);
	}

private:
	unsigned int clamp_size(unsigned int size);
	unsigned int size_dxtc(unsigned int width, unsigned int height);
	unsigned int size_rgb(unsigned int width, unsigned int height);

	// calculates 4-byte aligned width of image
	unsigned int get_dword_aligned_linesize(unsigned int width, unsigned int bpp) {
		return ((width * bpp + 31) & -32) >> 3;
	}

	void flip(CSurface &surface);
	void flip_texture(CTexture &texture);

	void write_texture(const CTexture &texture, std::ostream& os);

	unsigned int m_format;
	unsigned int m_components;
	TextureType m_type;
	bool m_valid;

	TArray<CTexture> m_images;
};

///////////////////////////////////////////////////////////////////////////////
// CDDSImage public functions

///////////////////////////////////////////////////////////////////////////////
// default constructor
CDDSImage::CDDSImage() :
	m_format(0), m_components(0), m_type(TextureNone), m_valid(false) {
}

CDDSImage::~CDDSImage() {
}

void CDDSImage::create_textureFlat(unsigned int format, unsigned int components, const CTexture &baseImage) {

	// remove any existing images
	clear();

	m_format = format;
	m_components = components;
	m_type = TextureFlat;

	m_images.Push(baseImage);

	m_valid = true;
}

void CDDSImage::create_texture3D(unsigned int format, unsigned int components, const CTexture &baseImage) {

	// remove any existing images
	clear();

	m_format = format;
	m_components = components;
	m_type = Texture3D;

	m_images.Push(baseImage);

	m_valid = true;
}

inline bool same_size(const CTexture &a, const CTexture &b) {
	if (a.get_width() != b.get_width())
		return false;
	if (a.get_height() != b.get_height())
		return false;
	if (a.get_depth() != b.get_depth())
		return false;

	return true;
}

void CDDSImage::create_textureCubemap(unsigned int format, unsigned int components, const CTexture &positiveX, const CTexture &negativeX,
	const CTexture &positiveY, const CTexture &negativeY, const CTexture &positiveZ, const CTexture &negativeZ) {



	// remove any existing images
	clear();

	m_format = format;
	m_components = components;
	m_type = TextureCubemap;

	m_images.Push(positiveX);
	m_images.Push(negativeX);
	m_images.Push(positiveY);
	m_images.Push(negativeY);
	m_images.Push(positiveZ);
	m_images.Push(negativeZ);

	m_valid = true;
}

///////////////////////////////////////////////////////////////////////////////
// loads DDS image
//
// filename - fully qualified name of DDS image
// flipImage - specifies whether image is flipped on load, default is true
void CDDSImage::load(const FString& filename, bool flipImage) {

	std::ifstream fs(*filename, std::ios::binary);
	load(fs, flipImage);
}

///////////////////////////////////////////////////////////////////////////////
// loads DDS image
//
// is - istream to read the image from
// flipImage - specifies whether image is flipped on load, default is true
void CDDSImage::load(std::istream& is, bool flipImage) {
	// clear any previously loaded images
	clear();

	// read in file marker, make sure its a DDS file
	char filecode[4];
	is.read(filecode, 4);
	if (strncmp(filecode, "DDS ", 4) != 0) {
		return;
	}

	// read in DDS header
	DDS_HEADER ddsh;
	is.read((char*)&ddsh, sizeof(DDS_HEADER));

	// default to flat texture type (1D, 2D, or rectangle)
	m_type = TextureFlat;

	// check if image is a cubemap
	if (ddsh.dwCaps2 & DDPF_FOURCC)
		m_type = TextureCubemap;

	// check if image is a volume texture
	if ((ddsh.dwCaps2 & DDSF_VOLUME) && (ddsh.dwDepth > 0))
		m_type = Texture3D;

	// figure out what the image format is
	if (ddsh.ddpf.dwFlags & DDPF_FOURCC) {
		switch (ddsh.ddpf.dwFourCC) {
		case FOURCC_DXT1:
			m_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			m_components = 3;
			break;
		case FOURCC_DXT3:
			m_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			m_components = 4;
			break;
		case FOURCC_DXT5:
			m_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			m_components = 4;
			break;
		default:
			break;
			//throw runtime_error("unknown texture compression '" + fourcc(ddsh.ddspf.dwFourCC) + "'");
		}
	}
	else if (ddsh.ddpf.dwRGBBitCount == 32 &&
		ddsh.ddpf.dwRBitMask == 0x00FF0000 &&
		ddsh.ddpf.dwGBitMask == 0x0000FF00 &&
		ddsh.ddpf.dwBBitMask == 0x000000FF &&
		ddsh.ddpf.dwABitMask == 0xFF000000) {
		m_format = GL_BGRA_EXT;
		m_components = 4;
	}
	else if (ddsh.ddpf.dwRGBBitCount == 32 &&
		ddsh.ddpf.dwRBitMask == 0x000000FF &&
		ddsh.ddpf.dwGBitMask == 0x0000FF00 &&
		ddsh.ddpf.dwBBitMask == 0x00FF0000 &&
		ddsh.ddpf.dwABitMask == 0xFF000000) {
		m_format = GL_RGBA;
		m_components = 4;
	}
	else if (ddsh.ddpf.dwRGBBitCount == 24 &&
		ddsh.ddpf.dwRBitMask == 0x000000FF &&
		ddsh.ddpf.dwGBitMask == 0x0000FF00 &&
		ddsh.ddpf.dwBBitMask == 0x00FF0000) {
		m_format = GL_RGB;
		m_components = 3;
	}
	else if (ddsh.ddpf.dwRGBBitCount == 24 &&
		ddsh.ddpf.dwRBitMask == 0x00FF0000 &&
		ddsh.ddpf.dwGBitMask == 0x0000FF00 &&
		ddsh.ddpf.dwBBitMask == 0x000000FF) {
		m_format = GL_BGR_EXT;
		m_components = 3;
	}
	else if (ddsh.ddpf.dwRGBBitCount == 8) {
		m_format = GL_LUMINANCE;
		m_components = 1;
	}
	else {
		//throw runtime_error("unknow texture format");
	}

	// store primary surface width/height/depth
	unsigned int width, height, depth;
	width = ddsh.dwWidth;
	height = ddsh.dwHeight;
	depth = clamp_size(ddsh.dwDepth);   // set to 1 if 0

										// use correct size calculation function depending on whether image is
										// compressed
	unsigned int (CDDSImage::*sizefunc)(unsigned int, unsigned int);
	sizefunc = (is_compressed() ? &CDDSImage::size_dxtc : &CDDSImage::size_rgb);

	// load all surfaces for the image (6 surfaces for cubemaps)
	for (unsigned int n = 0; n < (unsigned int)(m_type == TextureCubemap ? 6 : 1); n++) {
		// add empty texture object
		m_images.Push(CTexture());

		// get reference to newly added texture object
		CTexture &img = m_images[n];

		// calculate surface size
		unsigned int size = (this->*sizefunc)(width, height) * depth;

		// load surface
		uint8_t *pixels = new uint8_t[size];
		is.read((char*)pixels, size);

		img.create(width, height, depth, size, pixels);

		delete[] pixels;

		if (flipImage)
			flip(img);

		unsigned int w = clamp_size(width >> 1);
		unsigned int h = clamp_size(height >> 1);
		unsigned int d = clamp_size(depth >> 1);

		// store number of mipmaps
		unsigned int numMipmaps = ddsh.dwMipMapCount;

		// number of mipmaps in file includes main surface so decrease count
		// by one
		if (numMipmaps != 0)
			numMipmaps--;

		// load all mipmaps for current surface
		for (unsigned int i = 0; i < numMipmaps && (w || h); i++) {
			// add empty surface
			img.add_mipmap(CSurface());

			// get reference to newly added mipmap
			CSurface &mipmap = img.get_mipmap(i);

			// calculate mipmap size
			size = (this->*sizefunc)(w, h) * d;

			uint8_t *pixels = new uint8_t[size];
			is.read((char*)pixels, size);

			mipmap.create(w, h, d, size, pixels);

			delete[] pixels;

			if (flipImage)
				flip(mipmap);

			// shrink to next power of 2
			w = clamp_size(w >> 1);
			h = clamp_size(h >> 1);
			d = clamp_size(d >> 1);
		}
	}

	// swap cubemaps on y axis (since image is flipped in OGL)
	if (m_type == TextureCubemap && flipImage) {
		CTexture tmp;
		tmp = m_images[3];
		m_images[3] = m_images[2];
		m_images[2] = tmp;
	}

	m_valid = true;
}

void CDDSImage::write_texture(const CTexture &texture, std::ostream& os) {

	os.write((char*)(uint8_t*)texture, texture.get_size());

	for (unsigned int i = 0; i < texture.get_num_mipmaps(); i++) {
		const CSurface &mipmap = texture.get_mipmap(i);
		os.write((char*)(uint8_t*)mipmap, mipmap.get_size());
	}
}

void CDDSImage::save(const FString& filename, bool flipImage) {


	DDS_HEADER ddsh;
	unsigned int headerSize = sizeof(DDS_HEADER);
	memset(&ddsh, 0, headerSize);
	ddsh.dwSize = headerSize;
	ddsh.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	ddsh.dwHeight = get_height();
	ddsh.dwWidth = get_width();

	if (is_compressed()) {
		ddsh.dwFlags |= DDSD_LINEARSIZE;
		ddsh.dwLinearSize = get_size();
	}
	else {
		ddsh.dwFlags |= DDSD_PITCH;
		ddsh.dwLinearSize = get_dword_aligned_linesize(get_width(), m_components * 8);
	}

	if (m_type == Texture3D) {
		ddsh.dwFlags |= DDSD_DEPTH;
		ddsh.dwDepth = get_depth();
	}

	if (get_num_mipmaps() > 0) {
		ddsh.dwFlags |= DDSD_MIPMAPCOUNT;
		ddsh.dwMipMapCount = get_num_mipmaps() + 1;
	}

	ddsh.ddpf.dwSize = sizeof(DDS_PIXELFORMAT);

	if (is_compressed()) {
		ddsh.ddpf.dwFlags = DDPF_FOURCC;

		if (m_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
			ddsh.ddpf.dwFourCC = FOURCC_DXT1;
		if (m_format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
			ddsh.ddpf.dwFourCC = FOURCC_DXT3;
		if (m_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
			ddsh.ddpf.dwFourCC = FOURCC_DXT5;
	}
	else {
		ddsh.ddpf.dwFlags = (m_components == 4) ? DDSF_RGBA : DDPF_RGB;
		ddsh.ddpf.dwRGBBitCount = m_components * 8;
		ddsh.ddpf.dwRBitMask = 0x00ff0000;
		ddsh.ddpf.dwGBitMask = 0x0000ff00;
		ddsh.ddpf.dwBBitMask = 0x000000ff;

		if (m_components == 4) {
			ddsh.ddpf.dwFlags |= DDPF_ALPHAPIXELS;
			ddsh.ddpf.dwABitMask = 0xff000000;
		}
	}

	ddsh.dwCaps = DDSCAPS_TEXTURE;

	if (m_type == TextureCubemap) {
		ddsh.dwCaps |= DDSCAPS_COMPLEX;
		ddsh.dwCaps2 = DDSF_CUBEMAP | DDSF_CUBEMAP_ALL_FACES;
	}

	if (m_type == Texture3D) {
		ddsh.dwCaps |= DDSCAPS_COMPLEX;
		ddsh.dwCaps2 = DDSF_VOLUME;
	}

	if (get_num_mipmaps() > 0)
		ddsh.dwCaps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;

	// open file
	std::ofstream of;
	of.exceptions(std::ios::failbit);
	of.open(*filename, std::ios::binary);

	// write file header
	of.write("DDS ", 4);

	// write dds header
	of.write((char*)&ddsh, sizeof(DDS_HEADER));

	if (m_type != TextureCubemap) {
		CTexture tex = m_images[0];
		if (flipImage)
			flip_texture(tex);
		write_texture(tex, of);
	}
	else {


		for (int i = 0; i < m_images.Num(); i++) {
			CTexture cubeFace;

			if (i == 2)
				cubeFace = m_images[3];
			else if (i == 3)
				cubeFace = m_images[2];
			else
				cubeFace = m_images[i];

			if (flipImage)
				flip_texture(cubeFace);
			write_texture(cubeFace, of);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// free image memory
void CDDSImage::clear() {
	m_components = 0;
	m_format = 0;
	m_type = TextureNone;
	m_valid = false;

	m_images.Empty();
}


bool CDDSImage::is_compressed() {
	return (m_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
		|| (m_format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
		|| (m_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
}

///////////////////////////////////////////////////////////////////////////////
// clamps input size to [1-size]
inline unsigned int CDDSImage::clamp_size(unsigned int size) {
	if (size <= 0)
		size = 1;

	return size;
}

///////////////////////////////////////////////////////////////////////////////
// CDDSImage private functions
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// calculates size of DXTC texture in bytes
inline unsigned int CDDSImage::size_dxtc(unsigned int width, unsigned int height) {
	return ((width + 3) / 4) * ((height + 3) / 4) * (m_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8 : 16);
}

///////////////////////////////////////////////////////////////////////////////
// calculates size of uncompressed RGB texture in bytes
inline unsigned int CDDSImage::size_rgb(unsigned int width, unsigned int height) {
	return width * height * m_components;
}

///////////////////////////////////////////////////////////////////////////////
// flip image around X axis
void CDDSImage::flip(CSurface &surface) {
	unsigned int linesize;
	unsigned int offset;

	if (!is_compressed()) {

		unsigned int imagesize = surface.get_size() / surface.get_depth();
		linesize = imagesize / surface.get_height();

		uint8_t *tmp = new uint8_t[linesize];

		for (unsigned int n = 0; n < surface.get_depth(); n++) {
			offset = imagesize * n;
			uint8_t *top = (uint8_t*)surface + offset;
			uint8_t *bottom = top + (imagesize - linesize);

			for (unsigned int i = 0; i < (surface.get_height() >> 1); i++) {
				// swap
				memcpy(tmp, bottom, linesize);
				memcpy(bottom, top, linesize);
				memcpy(top, tmp, linesize);

				top += linesize;
				bottom -= linesize;
			}
		}

		delete[] tmp;
	}
	else {
		void(*flipblocks)(DXTColBlock*, unsigned int);
		unsigned int xblocks = surface.get_width() / 4;
		unsigned int yblocks = surface.get_height() / 4;
		unsigned int blocksize;

		switch (m_format) {
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			blocksize = 8;
			flipblocks = flip_blocks_dxtc1;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			blocksize = 16;
			flipblocks = flip_blocks_dxtc3;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			blocksize = 16;
			flipblocks = flip_blocks_dxtc5;
			break;
		default:
			return;
		}

		linesize = xblocks * blocksize;

		DXTColBlock *top;
		DXTColBlock *bottom;

		uint8_t *tmp = new uint8_t[linesize];

		for (unsigned int j = 0; j < (yblocks >> 1); j++) {
			top = (DXTColBlock*)((uint8_t*)surface + j * linesize);
			bottom = (DXTColBlock*)((uint8_t*)surface + (((yblocks - j) - 1) * linesize));

			flipblocks(top, xblocks);
			flipblocks(bottom, xblocks);

			// swap
			memcpy(tmp, bottom, linesize);
			memcpy(bottom, top, linesize);
			memcpy(top, tmp, linesize);
		}

		delete[] tmp;
	}
}

void CDDSImage::flip_texture(CTexture &texture) {
	flip(texture);

	for (unsigned int i = 0; i < texture.get_num_mipmaps(); i++) {
		flip(texture.get_mipmap(i));
	}
}

FColor RGBMEncode(FLinearColor Color)
{
	FColor Encoded;

	// Convert to gamma space
	Color.R = FMath::Sqrt(Color.R);
	Color.G = FMath::Sqrt(Color.G);
	Color.B = FMath::Sqrt(Color.B);

	// Range
	Color /= 16.0f;

	float MaxValue = FMath::Max(FMath::Max(Color.R, Color.G), FMath::Max(Color.B, DELTA));

	if (MaxValue > 0.75f)
	{
		// Fit to valid range by leveling off intensity
		float Tonemapped = (MaxValue - 0.75 * 0.75) / (MaxValue - 0.5);
		Color *= Tonemapped / MaxValue;
		MaxValue = Tonemapped;
	}

	Encoded.A = FMath::Min(FMath::CeilToInt(MaxValue * 255.0f), 255);
	Encoded.R = FMath::RoundToInt((Color.R * 255.0f / Encoded.A) * 255.0f);
	Encoded.G = FMath::RoundToInt((Color.G * 255.0f / Encoded.A) * 255.0f);
	Encoded.B = FMath::RoundToInt((Color.B * 255.0f / Encoded.A) * 255.0f);

	return Encoded;
}

void CubeFace2UNITY(int32 face, FMatrix &matrix, int32 size)
{
	FMatrix temp1, temp2, temp3;
	matrix.SetIdentity();
	temp1.SetIdentity();
	temp1.SetOrigin(FVector(-0.5f * (size - 1), -0.5f * (size - 1), 0.f));
	temp2.SetIdentity();
	temp3.SetIdentity();
	temp3.SetOrigin(FVector(0.5f * (size - 1), 0.5f * (size - 1), 0.f));
	switch (face)
	{
	case 0:
	{
		temp2.M[0][0] = 0;
		temp2.M[0][1] = -1;
		temp2.M[1][0] = 1;
		temp2.M[1][1] = 0;
		matrix = temp1*temp2*temp3;
	}
	break;
	case 1:
	{
		temp2.M[0][0] = 0;
		temp2.M[0][1] = 1;
		temp2.M[1][0] = -1;
		temp2.M[1][1] = 0;
		matrix = temp1*temp2*temp3;
	}
	break;
	case 3:
	{
		temp2.M[0][0] = 1;
		//temp2.M[0][1] = 1;
		//temp2.M[1][0] = -1;
		temp2.M[1][1] = -1;
		matrix = temp1*temp2*temp3;
	}
	break;
	case 5:
	{
		temp2.M[0][0] = -1;
		//temp2.M[0][1] = 1;
		//temp2.M[1][0] = -1;
		temp2.M[1][1] = -1;
		matrix = temp1*temp2*temp3;
	}
	break;
	}
}

static void EdgeWalkSetup(bool ReverseDirection, int32 Edge, int32 MipSize, int32& EdgeStart, int32& EdgeStep)
{
	if (ReverseDirection)
	{
		switch (Edge)
		{
		case EDGE_LEFT:		//start at lower left and walk up
			EdgeStart = MipSize * (MipSize - 1);
			EdgeStep = -MipSize;
			break;
		case EDGE_RIGHT:	//start at lower right and walk up
			EdgeStart = MipSize * (MipSize - 1) + (MipSize - 1);
			EdgeStep = -MipSize;
			break;
		case EDGE_TOP:		//start at upper right and walk left
			EdgeStart = (MipSize - 1);
			EdgeStep = -1;
			break;
		case EDGE_BOTTOM:	//start at lower right and walk left
			EdgeStart = MipSize * (MipSize - 1) + (MipSize - 1);
			EdgeStep = -1;
			break;
		}
	}
	else
	{
		switch (Edge)
		{
		case EDGE_LEFT:		//start at upper left and walk down
			EdgeStart = 0;
			EdgeStep = MipSize;
			break;
		case EDGE_RIGHT:	//start at upper right and walk down
			EdgeStart = (MipSize - 1);
			EdgeStep = MipSize;
			break;
		case EDGE_TOP:		//start at upper left and walk left
			EdgeStart = 0;
			EdgeStep = 1;
			break;
		case EDGE_BOTTOM:	//start at lower left and walk left
			EdgeStart = MipSize * (MipSize - 1);
			EdgeStep = 1;
			break;
		}
	}
}

void GetFaceData(TArray<uint8> &writeData, const uint8 *data, int32 face, int32 size)
{
	FMatrix matrix;
	CubeFace2UNITY(face, matrix, size);
	for (int32 y = 0; y < size; ++y)
	{
		for (int32 x = 0; x < size; ++x)
		{
			FVector v = matrix.TransformPosition(FVector(x, y, 0));
			writeData[v.Y*size * 4 + v.X * 4] = data[y*size * 4 + x * 4];
			writeData[v.Y*size * 4 + v.X * 4 + 1] = data[y*size * 4 + x * 4 + 1];
			writeData[v.Y*size * 4 + v.X * 4 + 2] = data[y*size * 4 + x * 4 + 2];
			writeData[v.Y*size * 4 + v.X * 4 + 3] = data[y*size * 4 + x * 4 + 3];
		}
	}
}

TRefCountPtr<FReflectionCaptureUncompressedData> GenerateFromDerivedDataSource(const FReflectionCaptureFullHDR& FullHDRData)
{
	const int32 NumMips = FMath::CeilLogTwo(FullHDRData.CubemapSize) + 1;

	TRefCountPtr<FReflectionCaptureUncompressedData> SourceCubemapData = FullHDRData.GetUncompressedData();

	int32 SourceMipBaseIndex = 0;
	int32 DestMipBaseIndex = 0;

	TRefCountPtr<FReflectionCaptureUncompressedData> CapturedData = new FReflectionCaptureUncompressedData(SourceCubemapData->Size() * sizeof(FColor) / sizeof(FFloat16Color));

	// Note: change REFLECTIONCAPTURE_ENCODED_DERIVEDDATA_VER when modifying the encoded data layout or contents

	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMips - MipIndex - 1);
		const int32 SourceCubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);
		const int32 DestCubeFaceBytes = MipSize * MipSize * sizeof(FColor);

		const FFloat16Color*	MipSrcData = (const FFloat16Color*)SourceCubemapData->GetData(SourceMipBaseIndex);
		FColor*					MipDstData = (FColor*)CapturedData->GetData(DestMipBaseIndex);

		// Fix cubemap seams by averaging colors across edges

		int32 CornerTable[4] =
		{
			0,
			MipSize - 1,
			MipSize * (MipSize - 1),
			MipSize * (MipSize - 1) + MipSize - 1,
		};

		// Average corner colors
		FLinearColor AvgCornerColors[8];
		memset(AvgCornerColors, 0, sizeof(AvgCornerColors));
		for (int32 Face = 0; Face < CubeFace_MAX; Face++)
		{
			const FFloat16Color* FaceSrcData = MipSrcData + Face * MipSize * MipSize;

			for (int32 Corner = 0; Corner < 4; Corner++)
			{
				AvgCornerColors[CubeCornerList[Face][Corner]] += FLinearColor(FaceSrcData[CornerTable[Corner]]);
			}
		}

		// Encode corners
		for (int32 Face = 0; Face < CubeFace_MAX; Face++)
		{
			FColor* FaceDstData = MipDstData + Face * MipSize * MipSize;

			for (int32 Corner = 0; Corner < 4; Corner++)
			{
				const FLinearColor LinearColor = AvgCornerColors[CubeCornerList[Face][Corner]] / 3.0f;
				FaceDstData[CornerTable[Corner]] = RGBMEncode(LinearColor);
			}
		}

		// Average edge colors
		for (int32 EdgeIndex = 0; EdgeIndex < 12; EdgeIndex++)
		{
			int32 FaceA = CubeEdgeListA[EdgeIndex][0];
			int32 EdgeA = CubeEdgeListA[EdgeIndex][1];

			int32 FaceB = CubeEdgeListB[EdgeIndex][0];
			int32 EdgeB = CubeEdgeListB[EdgeIndex][1];

			const FFloat16Color*	FaceSrcDataA = MipSrcData + FaceA * MipSize * MipSize;
			FColor*					FaceDstDataA = MipDstData + FaceA * MipSize * MipSize;

			const FFloat16Color*	FaceSrcDataB = MipSrcData + FaceB * MipSize * MipSize;
			FColor*					FaceDstDataB = MipDstData + FaceB * MipSize * MipSize;

			int32 EdgeStartA = 0;
			int32 EdgeStepA = 0;
			int32 EdgeStartB = 0;
			int32 EdgeStepB = 0;

			EdgeWalkSetup(false, EdgeA, MipSize, EdgeStartA, EdgeStepA);
			EdgeWalkSetup(EdgeA == EdgeB || EdgeA + EdgeB == 3, EdgeB, MipSize, EdgeStartB, EdgeStepB);

			// Walk edge
			// Skip corners
			for (int32 Texel = 1; Texel < MipSize - 1; Texel++)
			{
				int32 EdgeTexelA = EdgeStartA + EdgeStepA * Texel;
				int32 EdgeTexelB = EdgeStartB + EdgeStepB * Texel;

				check(0 <= EdgeTexelA && EdgeTexelA < MipSize * MipSize);
				check(0 <= EdgeTexelB && EdgeTexelB < MipSize * MipSize);

				const FLinearColor EdgeColorA = FLinearColor(FaceSrcDataA[EdgeTexelA]);
				const FLinearColor EdgeColorB = FLinearColor(FaceSrcDataB[EdgeTexelB]);
				const FLinearColor AvgColor = 0.5f * (EdgeColorA + EdgeColorB);

				FaceDstDataA[EdgeTexelA] = FaceDstDataB[EdgeTexelB] = RGBMEncode(AvgColor);
			}
		}

		// Encode rest of texels
		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			const int32 FaceSourceIndex = SourceMipBaseIndex + CubeFace * SourceCubeFaceBytes;
			const int32 FaceDestIndex = DestMipBaseIndex + CubeFace * DestCubeFaceBytes;
			const FFloat16Color* FaceSourceData = (const FFloat16Color*)SourceCubemapData->GetData(FaceSourceIndex);
			FColor* FaceDestData = (FColor*)CapturedData->GetData(FaceDestIndex);

			// Convert each texel from linear space FP16 to RGBM FColor
			// Note: Brightness on the capture is baked into the encoded HDR data
			// Skip edges
			for (int32 y = 1; y < MipSize - 1; y++)
			{
				for (int32 x = 1; x < MipSize - 1; x++)
				{
					int32 TexelIndex = x + y * MipSize;
					const FLinearColor LinearColor = FLinearColor(FaceSourceData[TexelIndex]);
					FaceDestData[TexelIndex] = RGBMEncode(LinearColor);
				}
			}
		}

		SourceMipBaseIndex += SourceCubeFaceBytes * CubeFace_MAX;
		DestMipBaseIndex += DestCubeFaceBytes * CubeFace_MAX;
	}
	return CapturedData;
}

void SaveTGA(const FString& filename, int32 width, int32 height, uint8* data)
{
	std::ofstream savefile;
	savefile.exceptions(std::ios::failbit);
	savefile.open(*filename, std::ios::binary);

	const char type_header[] = { 0,0,2,0,0,0,0,0,0,0,0,0 };
	savefile.write(&type_header[0], 12);

	const char header[] = { width % 256, width / 256, height % 256, height / 256, 32, 8 };

	unsigned int image_size = 4 * width * height;
	uint8* invert_data = new uint8[image_size];
	memset(invert_data, 0, image_size * sizeof(uint8));

	for (unsigned int i = 0; i < image_size; i++)
	{
		invert_data[i] = data[i];
	}

	// 垂直镜像
	//for (int i = 0; i < height; i++)
	//{
	//	for (int j = 0; j < width; j++)
	//	{
	//		int s = (i * width + j) * 4;
	//		int d = ((height - 1 - i) * width + j) * 4;

	//		data[s + 0] = invert_data[d + 0];
	//		data[s + 1] = invert_data[d + 1];
	//		data[s + 2] = invert_data[d + 2];
	//		data[s + 3] = invert_data[d + 3];
	//	}
	//}

	// 上下+左右镜像
	//for (int i = 0; i < height; i++)
	//{
	//	for (int j = 0; j < width; j++)
	//	{
	//		int s = (i * height + j) * 4;
	//		int d = ((width - 1 - i) * height - j) * 4;

	//		data[s + 0] = invert_data[d + 0];
	//		data[s + 1] = invert_data[d + 1];
	//		data[s + 2] = invert_data[d + 2];
	//		data[s + 3] = invert_data[d + 3];
	//	}
	//}

	// 水平
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			int s = (i * height + j) * 4;
			int d = (i * height + (width - j - 1)) * 4;

			data[s + 0] = invert_data[d + 0];
			data[s + 1] = invert_data[d + 1];
			data[s + 2] = invert_data[d + 2];
			data[s + 3] = invert_data[d + 3];
		}
	}

	savefile.write(&header[0], 6);
	savefile.write((char*)data, 4 * width * height);
	savefile.close();
}


void ExportReflectionProbes(const TArray<ReflectionInfo> &vReflectionProbes, const FString& kPath)
{
	TArray<uint8> writeData;
	for (auto& itProbe : vReflectionProbes)
	{
		TRefCountPtr<FReflectionCaptureUncompressedData> rpCubemapData = GenerateFromDerivedDataSource(*itProbe.m_pkData);
		TArray<uint8>& aryData = rpCubemapData->GetArray();
		int32 CubemapSize = itProbe.m_pkData->CubemapSize;
		if (aryData.Num())
		{
			writeData.Empty(aryData.Num());
			writeData.AddZeroed(aryData.Num());
			CTexture texarray[6];
			int32 MipMapCount = FMath::Log2(CubemapSize) + 1;
			int32 MipBaseIndex = 0;
			int32 MipSize = 1 << (MipMapCount - 1);
			int32 CubeFaceBytes = MipSize * MipSize * 4;
			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				writeData.AddZeroed(CubeFaceBytes);
				const int32 SourceIndex = MipBaseIndex + CubeFace * CubeFaceBytes;
				GetFaceData(writeData, aryData.GetData() + SourceIndex, CubeFace, MipSize);
				texarray[CubeFace].create(CubemapSize, CubemapSize, 1, CubeFaceBytes, writeData.GetData()/* EncodedData.GetData()+SourceIndex*/);
			}
			MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
		
			for (int32 MipIndex = 1; MipIndex < MipMapCount; MipIndex++)
			{
				MipSize = 1 << (MipMapCount - MipIndex - 1);
				CubeFaceBytes = MipSize * MipSize * 4;
				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					writeData.AddZeroed(CubeFaceBytes);
					const int32 SourceIndex = MipBaseIndex + CubeFace * CubeFaceBytes;
					GetFaceData(writeData, aryData.GetData() + SourceIndex, CubeFace, MipSize);
					CSurface surface(MipSize, MipSize, 1, CubeFaceBytes, writeData.GetData()/*EncodedData.GetData() + SourceIndex*/);
					texarray[CubeFace].add_mipmap(surface);
				}
				MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
			}

			//for (int i = 0; i < 6; ++i)
			//{
			//	SaveTGA(kPath + "/cube" + FString::FromInt(i) + ".tga", texarray[i].get_width(), texarray[i].get_height(), texarray[i].m_pixels);
			//}

			CDDSImage image;
			texarray[3].FlipX();
			image.create_textureCubemap(GL_BGRA_EXT, 4, texarray[0], texarray[1], texarray[5], texarray[4], texarray[2], texarray[3]);
			FString kExportPath = kPath + "/" + itProbe.m_strName + ".dds";
			image.save(kExportPath);
		}
	}
}

void FExportCubemapModule::PluginButtonClicked()
{
	TArray<UObject*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects(ASphereReflectionCapture::StaticClass(), /*out*/ SelectedActors);

	if (SelectedActors.Num() > 0)
	{
		auto pkPlatform = FDesktopPlatformModule::Get();
		if (!pkPlatform) 
			return;
		FString kPath;
		bool bRes = pkPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("Choose a folder to export", "Export").ToString(),
			FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
			kPath);
		if (!bRes)
		{
			return;
		}

		TArray<ReflectionInfo> m_aryReflectionProbes;
		for (int i = 0; i < SelectedActors.Num(); ++i)
		{
			AActor* pkActor = (AActor*)SelectedActors[i];

			TArray<UReflectionCaptureComponent*> aryCaptures;
			pkActor->GetComponents(aryCaptures);
			if (aryCaptures.Num() > 0 && aryCaptures[0])
			{
				const FReflectionCaptureFullHDR* pkData = aryCaptures[0]->GetFullHDRData();
				if (pkData)
				{
					ReflectionInfo& kInfo = m_aryReflectionProbes[m_aryReflectionProbes.AddDefaulted(1)];
					kInfo.m_strName = pkActor->GetName();
					kInfo.m_v3Position = aryCaptures[0]->GetComponentToWorld().ToMatrixWithScale().GetOrigin();
					kInfo.m_v3Offset = aryCaptures[0]->CaptureOffset;
					kInfo.m_fInfluenceRadius = aryCaptures[0]->GetInfluenceBoundingRadius();
					kInfo.m_fBrightness = aryCaptures[0]->Brightness;
					kInfo.m_fAverageBrightness = aryCaptures[0]->GetAverageBrightness();
					kInfo.m_pkData = pkData;
				}
			}
		}
		ExportReflectionProbes(m_aryReflectionProbes, kPath);
	}
}

void FExportCubemapModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FExportCubemapCommands::Get().PluginAction);
}

void FExportCubemapModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FExportCubemapCommands::Get().PluginAction);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FExportCubemapModule, ExportCubemap)