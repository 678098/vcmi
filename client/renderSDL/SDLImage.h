/*
 * SDLImage.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/GameConstants.h"

#ifdef IN
#undef IN
#endif

#ifdef OUT
#undef OUT
#endif

VCMI_LIB_NAMESPACE_BEGIN

class JsonNode;
class Rect;
class Point;

VCMI_LIB_NAMESPACE_END

struct SDL_Surface;
struct SDL_Color;
class CDefFile;
class ColorFilter;

/*
 * Wrapper around SDL_Surface
 */
class SDLImage : public IImage
{
public:
	
	const static int DEFAULT_PALETTE_COLORS = 256;
	
	//Surface without empty borders
	SDL_Surface * surf;
	//size of left and top borders
	Point margins;
	//total size including borders
	Point fullSize;

public:
	//Load image from def file
	SDLImage(CDefFile *data, size_t frame, size_t group=0);
	//Load from bitmap file
	SDLImage(std::string filename);

	SDLImage(const JsonNode & conf);
	//Create using existing surface, extraRef will increase refcount on SDL_Surface
	SDLImage(SDL_Surface * from, bool extraRef);
	~SDLImage();

	// Keep the original palette, in order to do color switching operation
	void savePalette();

	void draw(SDL_Surface * where, int posX=0, int posY=0, const Rect *src=nullptr) const override;
	void draw(SDL_Surface * where, const Rect * dest, const Rect * src) const override;
	std::shared_ptr<IImage> scaleFast(const Point & size) const override;
	void exportBitmap(const boost::filesystem::path & path) const override;
	void playerColored(PlayerColor player) override;
	void setFlagColor(PlayerColor player) override;
	bool isTransparent(const Point & coords) const override;
	Point dimensions() const override;

	void horizontalFlip() override;
	void verticalFlip() override;

	void shiftPalette(int from, int howMany) override;
	void adjustPalette(const ColorFilter & shifter, size_t colorsToSkip) override;
	void resetPalette(int colorID) override;
	void resetPalette() override;

	void setAlpha(uint8_t value) override;

	void setSpecialPallete(const SpecialPalette & SpecialPalette) override;

	friend class SDLImageLoader;

private:
	SDL_Palette * originalPalette;
};
