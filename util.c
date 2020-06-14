/*======================================================================*/
#include "util.h"


static int isspace_(int c){
	switch(c){
		case '\0':
		case '\t':
		case '\r':
		case '\n':
		case ' ':
			return 1;
	}
	return 0;
}


/*======================================================================*/
int strncpy_trim(char *dest, char *src, int len){
	char *end;

	dest[0] = '\0';

	while(0 < len){
		if(0 == isspace_(*src)){
			break;
		}
		src += 1;
		len -= 1;
	}
	if(0 >= len){
		return 0;
	}

	end = src;
	while((src + len) > end){
		if('\0' == (*end)){
			break;
		}
		end += 1;
	}
	end -= 1;

	while(src <= end){
		if(0 == isspace_(*end)){
			break;
		}
		end -= 1;
	}
	if(src > end){
		return 0;
	}

	strncat(dest, src, (end - src + 1));

	return (end - src + 1);
}


/*----------------------------------------------------------------------*/

int strncat_trim(char *dest, char *src, int len){
	dest += strlen(dest);
	return strncpy_trim(dest, src, len);
}


/*======================================================================*/
int title_formatting(char *buf, char *fmt, Music_Emu *emulator, char *path){
	char *p;
	track_info_t information;
	gme_track_info( emulator, &information, 0 );

	buf[0] = '\0';
	p = fmt;

	while('\0' != (*p)){

		if('%' != (*p)){
			char *q;
			q = strchr(p, '%');
			if(NULL == q){
				strcat(buf, p);
				p += strlen(p);
				continue;
			}
			strncat(buf, p, (q - p));
			p = q;
			continue;
		}

		p += 1;
		switch(*p){
			case '%':
				strcat(buf, "%");
				break;

			case 's':
			    strcat(buf,information.song);
				//strncat_trim(buf, information.song, sizeof(information.song));
				break;

			case 'S':{
					int t;
					t = strcat(buf,information.song);
					//strncat_trim(buf, information.song, sizeof(information.song));
					if(0 == t && (NULL != path)){
						char *q;
						q = strrchr(path, '\\');
						if(NULL == q){
							q = (path - 1);
						}
						strcat(buf, (q + 1));
					}
				}
				break;

			case 'g':
			    strcat(buf,information.game);
				//strncat_trim(buf, information.game, sizeof(information.game));
				break;

            case 'a':
                strcat(buf,information.author);
				//strncat_trim(buf, information.author, sizeof(information.author));
				break;
            case 'd':
                strcat(buf,information.dumper);
                break;
            case 'c':
                strcat(buf,information.system);
                break;

			default:
				strncat(buf, (p - 1), 2);
				break;
		}
		p += 1;
	}

	return 0;
}
