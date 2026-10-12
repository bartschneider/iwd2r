// iwd2r. Distributed under LICENSE.md.
#ifndef MAIN_H_
#define MAIN_H_

int iwd2r_main(int argc, char** argv);

// Whether the INI's [Program Options] Resolution is one iwd2r can run.
bool iwd2r_IsSupportedResolution(int width);

// CVideo::SCREENHEIGHT for a Resolution (CVideo::SCREENWIDTH), as
// CBaldurChitin's constructor derives it.
int iwd2r_ScreenHeightFor(int width);

#endif /* MAIN_H_ */
