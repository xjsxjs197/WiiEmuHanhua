#ifndef _GETTEXT_H_
#define _GETTEXT_H_

bool LoadLanguage(char *langBuf);

/*
 * input msg = a text in ASCII
 * output = the translated msg in utf-8
 */
extern "C" const char *gettext(const char *msg);

#endif /* _GETTEXT_H_ */
