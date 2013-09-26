#include <stdio.h>
#include <stdlib.h>

int main(int argc,char** argv)
{
    if(argc!=2) return -1;
    long num=atol(argv[1]);
    unsigned long a=1,b=2,i=0,c=0;
    for(i=0;i<num;i++){
        c=a+b;
        a=b;
        b=c;
    }
    printf("%lu\n",c);
}
