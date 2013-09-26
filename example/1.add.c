int a=0;
int b=1;
int main(int argc,char** argv)
{
    int n=10000;
    int i=0;
    for(i=0;i<n;i++){
        a=a+b;
        b=b+a;
    }
    return 0;
}
