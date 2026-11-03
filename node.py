import os,time,random,json,threading
from http.server import BaseHTTPRequestHandler,HTTPServer
import urllib.request
MY_ID=int(os.getenv("MY_ID","1"))
PORT=int(os.getenv("PORT","8080"))
LEFT_URL=os.getenv("LEFT_URL","")
RIGHT_URL=os.getenv("RIGHT_URL","")
x=random.uniform(-1,1)
left_x=0.0;right_x=0.0
class H(BaseHTTPRequestHandler):
 def do_GET(self):
  self.send_response(200)
  self.send_header("Content-type","application/json")
  self.end_headers()
  self.wfile.write(json.dumps({"id":MY_ID,"x":x}).encode())
 def log_message(self,*a):pass
def srv(): HTTPServer(("0.0.0.0",PORT),H).serve_forever()
def loop():
 global x,left_x,right_x
 for _ in range(5000):
  try:
   if MY_ID!=1 and LEFT_URL: left_x=json.loads(urllib.request.urlopen(LEFT_URL,timeout=1).read())["x"]
   else: left_x=0.0
  except: pass
  try:
   if MY_ID!=35 and RIGHT_URL: right_x=json.loads(urllib.request.urlopen(RIGHT_URL,timeout=1).read())["x"]
   else: right_x=0.0
  except: pass
  x=x-0.25*(2*x-left_x-right_x)
  time.sleep(0.05)
threading.Thread(target=srv,daemon=True).start()
loop()
