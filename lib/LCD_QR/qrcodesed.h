#pragma once

#include "qrcodedisplay.h"
#include "SED1530_LCD.h"

class QRcode_SED1530 : public QRcodeDisplay
{
	private:
        SED1530_LCD *display;
        void drawPixel(int x, int y, int color);
	public:
		QRcode_SED1530(SED1530_LCD *display);

		void init();
		void screenwhite();
		void screenupdate();
		int  getOffsetX() const { return offsetsX; }
};
