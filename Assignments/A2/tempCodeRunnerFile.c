f(strcmp(op,"pwd")==0){
        if(getcwd(ans,argmax_size)==NULL){
            printf("Error in execution\n");
        }
        else{
            printf("\t%s\n",ans);
        }
    }