/* Compile against the original compiler and VM; host instrumentation only. */
#define main original_reproduction_main
#include "migration_reproduce.c"
#undef main
#ifdef _WIN32
#include <windows.h>
static double now_ns(void) { LARGE_INTEGER t,f; QueryPerformanceCounter(&t); QueryPerformanceFrequency(&f); return (double)t.QuadPart*1e9/(double)f.QuadPart; }
#else
#include <time.h>
static double now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (double)t.tv_sec*1e9+t.tv_nsec; }
#endif
struct observation { uint64_t hash; unsigned count; };
static struct observation observer;
static void observe(uint32_t x) { observer.hash=(observer.hash ^ x)*1099511628211ULL; observer.count++; }
static uint32_t number(struct ccbpf_program *p,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{ (void)p;(void)b;(void)c;(void)d; observe(a);return 0; }
static uint32_t string(struct ccbpf_program *p,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{ (void)b;(void)c;(void)d; REQUIRE(a<(uint32_t)p->string_count); for(const unsigned char *s=(const unsigned char*)p->strings[a];*s;s++) observe(*s); observe(256);return 0; }
static enum ccbpf_status finish(struct ccbpf_ctx *s,struct ccbpf_program *p,int budget)
{ for(unsigned i=0;i<100000;i++){ enum ccbpf_status r=ccbpf_vm_step(s,p,NULL,0,0,budget); if(r==CCBPF_FINISHED || r==CCBPF_ERROR)return r; } REQUIRE(0);return CCBPF_ERROR; }
static volatile uint32_t sink;
int main(int argc,char **argv)
{
 REQUIRE(argc==4); unsigned expected=(unsigned)strtoul(argv[2],NULL,10); int bench=atoi(argv[3]);
 size_t len; uint8_t *image=compile_image(argv[1],&len);
 ccbpf_system_init();native_register(3,1,number);native_register(4,1,string);native_register(8,0,native_migrate);
 size_t free_before=heap_get_stats().remain_size;
 struct ccbpf_program *prog=ccbpf_load_from_memory(image,len);REQUIRE(prog);
 size_t free_loaded=heap_get_stats().remain_size;
 struct ccbpf_ctx *states=calloc(4097,sizeof(*states));
 struct observation *obs=calloc(4097,sizeof(*obs));REQUIRE(states && obs);
 observer=(struct observation){1469598103934665603ULL,0};obs[0]=observer;
 unsigned steps=0,yields=0;enum ccbpf_status status;
 do { REQUIRE(steps<4096); states[steps+1]=states[steps];
  status=ccbpf_vm_step(&states[steps+1],prog,NULL,0,0,1);REQUIRE(status!=CCBPF_ERROR);
  if(status==CCBPF_MIGRATE)yields++;steps++;obs[steps]=observer;
 }while(status!=CCBPF_FINISHED);
 REQUIRE(states[steps].ret==expected);
 for(unsigned k=0;k<steps;k++) {
  uint8_t *buf;size_t size;REQUIRE(ccbpf_ctx_pack(&states[k],&buf,&size)==0);
  struct ccbpf_ctx dst;memset(&dst,0xa5,sizeof(dst));
  REQUIRE(ccbpf_ctx_unpack(&dst,buf,size)==0);REQUIRE(memcmp(&dst,&states[k],size)==0);
  memset(buf,0x5a,size);heap_free(buf);
  struct ccbpf_program *target=ccbpf_load_from_memory(image,len);REQUIRE(target);
  observer=obs[k];REQUIRE(finish(&dst,target,64)==CCBPF_FINISHED);
  REQUIRE(memcmp(&dst,&states[steps],sizeof(dst))==0);
  REQUIRE(observer.hash==obs[steps].hash && observer.count==obs[steps].count);
  ccbpf_unload(target);
 }
 /* Repeated move after every instruction, using a fresh loaded program each time. */
 struct ccbpf_ctx moving={0};observer=obs[0];
 for(unsigned k=0;k<steps;k++) {
  uint8_t *buf;size_t size;REQUIRE(ccbpf_ctx_pack(&moving,&buf,&size)==0);
  memset(&moving,0xa5,sizeof(moving));REQUIRE(ccbpf_ctx_unpack(&moving,buf,size)==0);heap_free(buf);
  struct ccbpf_program *target=ccbpf_load_from_memory(image,len);REQUIRE(target);
  status=ccbpf_vm_step(&moving,target,NULL,0,0,1);REQUIRE(status!=CCBPF_ERROR);ccbpf_unload(target);
 }
 REQUIRE(status==CCBPF_FINISHED && memcmp(&moving,&states[steps],sizeof(moving))==0);
 REQUIRE(observer.hash==obs[steps].hash && observer.count==obs[steps].count);
 /* Codec rejection and detection boundaries. No invalid pc is executed. */
 struct ccbpf_ctx dst={0},zero={0};uint8_t *buf;size_t size;
 REQUIRE(ccbpf_ctx_pack(&states[0],&buf,&size)==0);
 REQUIRE(ccbpf_ctx_unpack(&dst,buf,0)==-1);
 REQUIRE(ccbpf_ctx_unpack(&dst,buf,size-1)==-1);
 REQUIRE(ccbpf_ctx_unpack(&dst,buf,size+1)==-1);
 REQUIRE(memcmp(&dst,&zero,sizeof(dst))==0);
 buf[sizeof(dst)-1]^=1;REQUIRE(ccbpf_ctx_unpack(&dst,buf,size)==0);
 REQUIRE(memcmp(&dst,&states[0],sizeof(dst))!=0);
 struct ccbpf_ctx invalid=states[0];invalid.pc=0xffffffffU;
 REQUIRE(ccbpf_ctx_unpack(&dst,(const uint8_t*)&invalid,sizeof(invalid))==0);heap_free(buf);
 uint8_t saved_magic=image[0];image[0]^=1;REQUIRE(ccbpf_load_from_memory(image,len)==NULL);image[0]=saved_magic;
 REQUIRE(ccbpf_load_from_memory(image,1)==NULL);
 printf("RESULT,%s,%zu,%zu,%u,%u,%u,%u\n",argv[1],prog->insn_count,len,steps,steps,yields,expected);
 if(bench) {
  struct ccbpf_ctx checkpoint=states[steps/2];uint8_t *stable;size_t stable_len;
  REQUIRE(ccbpf_ctx_pack(&checkpoint,&stable,&stable_len)==0);
  for(int kind=0;kind<4;kind++)for(int sample=-3;sample<31;sample++){
   int n=kind==2?100:1000;double start=now_ns();
   for(int j=0;j<n;j++){
    if(kind==0){uint8_t *b;size_t z;REQUIRE(ccbpf_ctx_pack(&checkpoint,&b,&z)==0);sink+=b[0];heap_free(b);}
    if(kind==1){REQUIRE(ccbpf_ctx_unpack(&dst,stable,stable_len)==0);sink+=dst.pc;}
    if(kind==2){struct ccbpf_program *q=ccbpf_load_from_memory(image,len);REQUIRE(q);sink+=(uint32_t)q->insn_count;ccbpf_unload(q);}
    if(kind==3){struct ccbpf_ctx s={0};observer=obs[0];REQUIRE(finish(&s,prog,64)==CCBPF_FINISHED);sink+=s.ret;}
   }
   double elapsed=now_ns()-start;if(sample>=0)printf("TIMING,%s,%d,%d,%d,%.3f\n",argv[1],kind,sample,n,elapsed/n);
  }heap_free(stable);
 }
 REQUIRE(heap_get_stats().remain_size==free_loaded);
 ccbpf_unload(prog);REQUIRE(heap_get_stats().remain_size==free_before);
 printf("HEAP,%s,%zu,%zu\n",argv[1],free_before-free_loaded,sizeof(struct ccbpf_ctx));
 free(image);free(states);free(obs);return 0;
}
