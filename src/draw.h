#ifndef __draw_h__
#define __draw_h__

class draw{
public:
	draw();
	~draw(){};
	int draw_text(unsigned char *image, unsigned int startx, unsigned int starty, unsigned int width, const char *text, unsigned int factor);
	int draw_textn(unsigned char *image, unsigned int startx, unsigned int starty, unsigned int width, const char *text, int len, unsigned int factor);
private:
	int initialize_chars(void);

};





#endif//