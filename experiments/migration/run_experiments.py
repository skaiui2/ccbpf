from pathlib import Path
import os,sys,subprocess,json,csv,random,hashlib,statistics,platform,argparse
parser=argparse.ArgumentParser();parser.add_argument('--cc',default='gcc');parser.add_argument('--out',required=True);args=parser.parse_args()
E=Path(__file__).resolve().parent;R=E.parent.parent;O=Path(args.out).resolve();O.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['PATH']=str(Path(args.cc).resolve().parent)+os.pathsep+env['PATH']
cases=[('readme',(R/'migrate.bpf').read_text(),0,True),('hardware','int hook(void *ctx){int a;a=1;print(a);a=a+1;print(a);a=a+1;print(a);a=a+1;print(a);migrate();a=a+1;print(a);a=a+1;print(a);a=a+1123;print(a);return a;}',1129,True)]
for name,body,result in [('early','migrate();a=7;return a;',7),('late','a=23;migrate();return a;',23),('twice','a=3;migrate();a=a*7;migrate();a=a+2;return a;',23),('branch_true','a=7;if(a>3){a=a+5;}else{a=a+2;}migrate();return a;',12),('branch_false','a=1;if(a>3){a=a+5;}else{a=a+2;}migrate();return a;',3),('strings','a=19;print_str("before");migrate();print_str("after");print(a);return a;',19)]:
 cases.append((name,'int hook(void *ctx){int a;'+body+'}',result,False))
for seed in range(24):
 rng=random.Random(seed);value=rng.randint(1,100);body=['int hook(void *ctx){int a;',f'a={value};']
 for j in range(12):
  b=rng.randint(1,13);op=rng.choice(['+','-','*','/','%'])
  if op=='-' and value<b:op='+'
  body.append(f'a=a{op}{b};')
  value={'+':lambda:(value+b)&0xffffffff,'-':lambda:(value-b)&0xffffffff,'*':lambda:(value*b)&0xffffffff,'/':lambda:value//b,'%':lambda:value%b}[op]()
  if j in (3,8):body.append('migrate();')
 body.append('print(a);return a;}')
 cases.append((f'arithmetic_{seed:02d}','\n'.join(body),value,seed==0))
includes=['ccBPF','ccBPF/vm/bpf/include','ccBPF/compiler/frontend/include','ccBPF/compiler/backend/include','ccBPF/compiler/ir/include','mg/include','lib/include']
dirs=['ccBPF/compiler/frontend/source','ccBPF/compiler/backend/source','ccBPF/compiler/ir/source','ccBPF/vm/bpf/source','mg/source','lib/source']
rows=[];timings=[];heaps=[];failures=[];commands=[]
for opt in ['Os','O2']:
 exe=O/('suite-'+opt+('.exe' if os.name=='nt' else ''))
 cmd=[args.cc,'-std=c99','-'+opt,'-include','string.h','-D_POSIX_C_SOURCE=200809L']+[f'-I{R/p}' for p in includes]+[str(E/'suite.c')]+[str(p) for d in dirs for p in (R/d).glob('*.c')]+['-lm','-o',str(exe)]
 commands.append(cmd);p=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=90);(O/f'build-{opt}.log').write_text(p.stdout+p.stderr)
 if p.returncode:raise RuntimeError(p.stderr)
 for name,source,oracle,bench in cases:
  file=O/(name+'.bpf');file.write_text(source,encoding='utf-8',newline='\n')
  try:
   p=subprocess.run([str(exe),str(file),str(oracle),str(int(bench))],capture_output=True,text=True,env=env,timeout=45)
   (O/f'{opt}-{name}.log').write_text(p.stdout+p.stderr,encoding='utf-8')
   if p.returncode:failures.append([opt,name,p.returncode,p.stderr]);continue
   for line in p.stdout.splitlines():
    if line.startswith('RESULT,'):rows.append([opt,name]+line.split(',')[2:])
    if line.startswith('TIMING,'):timings.append([opt,name]+line.split(',')[2:])
    if line.startswith('HEAP,'):heaps.append([opt,name]+line.split(',')[2:])
  except subprocess.TimeoutExpired:failures.append([opt,name,'timeout'])
  print(opt,name,'done',flush=True)
for name,header,data in [('correctness',['optimization','workload','instructions','image_bytes','executed_steps','checkpoints','explicit_yields','return'],rows),('timings',['optimization','workload','operation','sample','batch_size','ns_per_operation'],timings),('heap',['optimization','workload','loaded_program_allocator_bytes','context_bytes'],heaps)]:
 with (O/(name+'.csv')).open('w',newline='') as f:w=csv.writer(f);w.writerow(header);w.writerows(data)
process_exe=O/('process-test'+('.exe' if os.name=='nt' else ''))
process_cmd=[str(E/'process.c') if x==str(E/'suite.c') else str(process_exe) if x==str(exe) else x for x in cmd]
build=subprocess.run(process_cmd,capture_output=True,text=True,env=env,timeout=90)
(O/'build-process.log').write_text(build.stdout+build.stderr)
if build.returncode:raise RuntimeError(build.stderr)
process_passes=0
for attempt in range(10):
 handoff=O/f'handoff-{attempt}.bin'
 send=subprocess.run([str(process_exe),'send',str(R/'migrate.bpf'),str(handoff)],capture_output=True,text=True,env=env,timeout=30)
 recv=subprocess.run([str(process_exe),'receive',str(R/'migrate.bpf'),str(handoff)],capture_output=True,text=True,env=env,timeout=30)
 (O/f'process-{attempt}-source.log').write_text(send.stdout+send.stderr)
 (O/f'process-{attempt}-destination.log').write_text(recv.stdout+recv.stderr)
 import re
 good=send.returncode==0 and recv.returncode==0 and [int(x) for x in re.findall(r'^SOURCE: (\d+)$',send.stdout,re.M)]==list(range(1,6)) and [int(x) for x in re.findall(r'^DESTINATION: (\d+)$',recv.stdout,re.M)]==list(range(6,12)) and 'RESUMED ret=0' in recv.stdout
 if good:process_passes+=1
 else:failures.append(['process',attempt,send.returncode,recv.returncode])
stats=[]
for opt in ['Os','O2']:
 for workload in ['readme','hardware','arithmetic_00']:
  for kind,label in enumerate(['pack_and_free','unpack','load_and_unload','execute']):
   values=sorted(float(r[-1]) for r in timings if r[0]==opt and r[1]==workload and int(r[2])==kind)
   if values:stats.append(dict(optimization=opt,workload=workload,operation=label,samples=len(values),median_ns=statistics.median(values),p05_ns=values[int(.05*(len(values)-1))],p95_ns=values[int(.95*(len(values)-1))]))
result=dict(platform=platform.platform(),processor=os.environ.get('PROCESSOR_IDENTIFIER',platform.processor()),compiler=subprocess.check_output([args.cc,'--version'],text=True).splitlines()[0],revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),cases=len(cases),successful_runs=len(rows),checkpoint_tests=sum(int(r[5]) for r in rows),failures=failures,timing_summary=stats,compile_commands=commands,files={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [E/'suite.c',E/'run_experiments.py',E/'migration_reproduce.c',R/'migrate.bpf']})
result['fresh_process_transfer_passes']=process_passes
result['files'][str((E/'process.c').relative_to(R))]=hashlib.sha256((E/'process.c').read_bytes()).hexdigest()
result['compile_commands'].append(process_cmd)
(O/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ['compile_commands','timing_summary','files']},indent=2))
sys.exit(bool(failures))
