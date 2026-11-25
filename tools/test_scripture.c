/* JesusSaves v1.1.1 - native unit test for desktop/scripture.c.  Build and run from netsurf/:
 *   gcc -fsanitize=address,undefined -I. -o /tmp/t ../tools/test_scripture.c desktop/scripture.c desktop/scripture_kjv.c -lz && /tmp/t */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "desktop/scripture.h"
static int fails;
#define OK(c) do{ if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} }while(0)
static int P(const char*s, int b,int c,int v,int e){ scripture_ref r; if(!scripture_parse(s,strlen(s),&r)) return 0;
  return r.book==b&&r.chapter==c&&r.vstart==v&&r.vend==e; }
static int N(const char*s){ scripture_ref r; return !scripture_parse(s,strlen(s),&r); }
int main(void){
 char b[600]; scripture_ref r; char *u;
 OK(P("John 3:16",43,3,16,16)); OK(P("john3:16",43,3,16,16)); OK(P("  John 3:16.  ",43,3,16,16));
 OK(P("Jn 14:6",43,14,6,6)); OK(P("1 John 1:9",62,1,9,9)); OK(P("1John 1:9",62,1,9,9)); OK(P("First John 1:9",62,1,9,9));
 OK(P("1st Cor 13:4-7",46,13,4,7)); OK(P("1 Cor. 13:4\xE2\x80\x93" "7",46,13,4,7)); OK(P("Ps 23",19,23,0,0));
 OK(P("Psalm 23:1-6",19,23,1,6)); OK(P("Song of Solomon 2:1",22,2,1,1)); OK(P("Jude 3",65,1,3,3)); OK(P("Jude 1:3",65,1,3,3));
 OK(P("Rom 10:9-10",45,10,9,10)); OK(P("Acts 4:12",44,4,12,12)); OK(P("Revelation 22:21",66,22,21,21));
 OK(P("Gen 1:1-99",1,1,1,31)); OK(P("2 Tim 3:16",55,3,16,16)); OK(P("3 John 4",64,1,4,4));
 /* rejections: must go to web search / address handling */
 OK(N("jesus")); OK(N("John")); OK(N("ti 84")); OK(N("John 99:1")); OK(N("John 3:99")); OK(N("John 3:0"));
 OK(N("John 3:16 is great")); OK(N("is 5")); OK(N("am 5")); OK(N("")); OK(N("1 2:3")); OK(N("John 3:5-2"));
 OK(N("example.com")); OK(N("https://john.com/3:16")); OK(N("Matthew 29")); OK(N("Obadiah 2:1")); OK(N("John 1000:1"));
 OK(N("iphone 15")); OK(N("ex 3:2"));
 /* text */
 OK(P("John 3:16",43,3,16,16)); scripture_parse("John 3:16",9,&r);
 OK(scripture_text(&r,b,sizeof b)>0); printf("%s\n",b);
 OK(strstr(b,"For God so loved the world")!=NULL && strstr(b,"everlasting life.")!=NULL);
 scripture_parse("John 14:6",9,&r); scripture_text(&r,b,sizeof b); printf("%s\n",b);
 OK(strcmp(b,"Jesus saith unto him, I am the way, the truth, and the life: no man cometh unto the Father, but by me.")==0);
 scripture_parse("Acts 4:12",9,&r); scripture_text(&r,b,sizeof b); printf("%s\n",b);
 OK(strcmp(b,"Neither is there salvation in any other: for there is none other name under heaven given among men, whereby we must be saved.")==0);
 scripture_parse("Rom 3:23-24",11,&r); scripture_text(&r,b,sizeof b); printf("%s\n",b);
 scripture_parse("Ps 23",5,&r); scripture_text(&r,b,60); printf("[trunc60] %s\n",b); OK(strlen(b)<=59 && strstr(b,"...")!=NULL);
 scripture_parse("1 Cor 13:4-7",12,&r); u=scripture_url(&r); printf("%s\n",u);
 OK(strcmp(u,"https://www.biblegateway.com/passage/?search=1%20Corinthians%2013%3A4-7&version=KJV")==0); free(u);
 /* preview via link text and via url */
 OK(scripture_preview("John 3:16",NULL,b,sizeof b)); printf("%s\n",b);
 OK(scripture_preview("click here","https://www.biblegateway.com/passage/?search=John%2014%3A6&version=KJV",b,sizeof b)); printf("%s\n",b);
 OK(strncmp(b,"John 14:6 (KJV): Jesus saith",28)==0);
 OK(scripture_preview("click here","https://www.esv.org/1+John+1:9/",b,sizeof b)); printf("%s\n",b);
 OK(!scripture_preview("Read more","https://example.com/john/3",b,sizeof b));
 OK(!scripture_preview("Home","https://john3:16.example.com/",b,sizeof b));
 scripture_finalise(); OK(P("John 3:16",43,3,16,16)); scripture_finalise();
 printf(fails?"FAILURES: %d\n":"ALL TESTS PASSED\n",fails); return fails!=0;
}
