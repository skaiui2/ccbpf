/* Fresh-process continuation through a file. This is a test transport. */
#define main original_reproduction_main
#include "migration_reproduce.c"
#undef main
static void put32(FILE *f,uint32_t n) { unsigned char b[4]={(unsigned char)n,(unsigned char)(n>>8),(unsigned char)(n>>16),(unsigned char)(n>>24)};REQUIRE(fwrite(b,1,4,f)==4); }
static uint32_t get32(FILE *f) { unsigned char b[4];REQUIRE(fread(b,1,4,f)==4);return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24); }
int main(int argc,char **argv)
{
 REQUIRE(argc==4);int send=strcmp(argv[1],"send")==0;
 REQUIRE(send || strcmp(argv[1],"receive")==0);
 uint8_t *img,*snapshot=NULL;size_t image_len,context_len;struct ccbpf_ctx state={0};
 if(send)img=compile_image(argv[2],&image_len);
 else {
  FILE *f=fopen(argv[3],"rb");REQUIRE(f);REQUIRE(get32(f)==0x4d424343U);
  image_len=get32(f);context_len=get32(f);
  REQUIRE(image_len>0 && image_len<=65536 && context_len==sizeof(state));
  img=malloc(image_len);snapshot=malloc(context_len);REQUIRE(img && snapshot);
  REQUIRE(fread(img,1,image_len,f)==image_len);REQUIRE(fread(snapshot,1,context_len,f)==context_len);
  REQUIRE(fgetc(f)==EOF);fclose(f);
  REQUIRE(ccbpf_ctx_unpack(&state,snapshot,context_len)==0);free(snapshot);
 }
 ccbpf_system_init();native_register(3,1,print_number);native_register(4,1,print_string);native_register(8,0,native_migrate);
 struct ccbpf_program *prog=ccbpf_load_from_memory(img,image_len);REQUIRE(prog);
 REQUIRE(state.pc<prog->insn_count);phase=send?"SOURCE":"DESTINATION";
 enum ccbpf_status status=until_stop(&state,prog);
 if(send){
  REQUIRE(status==CCBPF_MIGRATE);REQUIRE(ccbpf_ctx_pack(&state,&snapshot,&context_len)==0);
  FILE *f=fopen(argv[3],"wb");REQUIRE(f);put32(f,0x4d424343U);put32(f,(uint32_t)image_len);put32(f,(uint32_t)context_len);
  REQUIRE(fwrite(img,1,image_len,f)==image_len);REQUIRE(fwrite(snapshot,1,context_len,f)==context_len);REQUIRE(fclose(f)==0);
  heap_free(snapshot);printf("SAVED pc=%u\n",state.pc);
 }else{REQUIRE(status==CCBPF_FINISHED && state.ret==0);printf("RESUMED ret=%u\n",state.ret);}
 ccbpf_unload(prog);free(img);return 0;
}
