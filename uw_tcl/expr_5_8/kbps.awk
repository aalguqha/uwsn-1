BEGIN{

}{
    value = $1
    if($1 == 0){
        printf("\n");
    }else{
        printf("%d %f\n",$1,$1/(1024*8));
    }
}END{

}
