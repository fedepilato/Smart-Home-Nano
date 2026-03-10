import json
import time
import paho.mqtt.client as mqtt
import cherrypy
import requests
from MyMQTT import MyMQTT

url = "http://192.168.194.120:8080/services/subscription/Broker-Mqtt"
registrationUrl = "http://192.168.194.120:8080/devices/subscription"
personalData = {
    'deviceID': 12342321,
    'endpoint' : "/tiot/0"
}
baseTopic = "tiot/0/"
flag = True

class Entity():
    def __init__(self, clientID, brokerId, port):
        # create an instance of MyMQTT class
        self.topic = ""
        self.cnt = 0
        self.flag = False
        self.clientID = clientID
        self._isSubscribe = False
        self.myMqttClient = MyMQTT(self.clientID, brokerId , port, self)
    
    def run(self):
        print ("running %s" % (self.clientID)) 
        self.myMqttClient.start()

    def end(self):
        print ("ending %s" % (self.clientID))
        self.myMqttClient.stop()
    def Subscribe(self, topic):
        self.topic = topic
        self._isSubscribe = True
        self.myMqttClient.mySubscribe(topic)
    def Publish(self, topic, value, name):
        msg = {
            "bn": "Home",
            "e" : [{
                "n" : name,
                "v" : value,
                "t" : time.time(),
                "u" : "None"
            }]
        }
        print(f"Sto pubblicando sotto il topic : {topic}")
        self.myMqttClient.myPublish(topic, json.dumps(msg))
    def notify(self, topic, msg):
        if (self._isSubscribe):
            diz = json.loads(msg)
            if (diz['e'][0]['t'] - time.time() < 120): # se è stato pubblicato negli ultimi 2 minuti
                print(f"Il valore attuale di {diz['e'][0]['n']} è: {diz['e'][0]['v']} [ Topic = {topic} ]")
                if (self.topic == "/tiot/0/+"):
                    self.cnt+=1
                    if (self.cnt >= 3):
                        self.flag = False
                else:
                    self.flag = False
        
    def UnSubscribe(self):
        self.myMqttClient.unsubscribe()




if __name__ == "__main__":
    response = requests.post(registrationUrl, json=personalData)
    response = requests.get(url)
    data = response.json()
    Home = Entity("1221", data['Broker'],data["Port"])
    Home.run()
    menu = -1
    while(menu != 5):
        Home.UnSubscribe()
        menu = input("Digitare:\n 1) per ricevere informazioni\n 2) per mandare comandi di attuazione\n 3) per stampare un messaggio sul display\n 4)Exit\n")
        match(int (menu)):
            case 1:
                clt = 0
                while (clt != 4):
                    Home.UnSubscribe()
                    baseTopic = "/tiot/0/"
                    clt = input("Quale informazioni vuoi sapere\n0. Tutto\n1. Temperatura\n2. Presenza\n3. Rumore\n4.Led\n5.Exit\n")    
                    match(int(clt)):
                        case 0:
                            baseTopic = baseTopic +"+"
                            Home.cnt = 0
                            Home.Subscribe(baseTopic)
                            Home.flag = True
                            while(Home.flag == True):
                                val = ""
                        case 1:
                            baseTopic = baseTopic + "temperature"
                            Home.Subscribe(baseTopic)
                            Home.flag = True
                            while(Home.flag == True):
                                val = ""
                        case 2:
                            baseTopic = baseTopic + "presence"
                            Home.Subscribe(baseTopic)
                            Home.flag = True
                            while(Home.flag ==True):
                                val = ""
                        case 3:
                            baseTopic = baseTopic + "noise"
                            Home.Subscribe(baseTopic)
                            Home.flag = True
                            while(Home.flag == True):
                                val = ""
                        case 4:
                            baseTopic = baseTopic + "led"
                            Home.Subscribe(baseTopic)
                            Home.flag = True
                            while(Home.flag == True):
                                val = ""
                        case 5:
                            print("Arrivederci!\n")

                        case _:
                            print("Comando non valido!\n")
            case 2:
                clt = 0
                while(int(clt) != 3):
                    Home.UnSubscribe()
                    baseTopic = "/tiot/0/"
                    clt = input("Quale informazioni vuoi sapere\n1. Led\n2. Fan\n3.Exit\n")
                    match(int(clt)):
                        case 1:
                            baseTopic = baseTopic + "led"
                            msg = input("Digitare 1 per accendere il led oppure 0 per spegnerlo: \n")
                            if msg == "1":
                                Home.Publish(baseTopic, 1, "led")
                            elif msg == "0":
                                 Home.Publish(baseTopic, 0, "led")
                            else :
                                print("Comando errato!\n")
                        case 2:
                            baseTopic = baseTopic + "fan"
                            msg = input("Digitare 1 per aumentare la ventola oppure 0 per diminuirla: \n")
                            if msg == "1":
                                Home.Publish(baseTopic, 1, "fan")
                            elif msg == "0":
                                Home.Publish(baseTopic, 0, "fan")
                            else :
                                print("Comando non valido!\n")
                        case 3: 
                            print("Arrivederci!\n")
                        case _:
                            print("Comando non valido!\n")
            case 3: 
                Home.UnSubscribe()
                baseTopic = "/tiot/0/"
                msg = "--------------------------------------------"
                while (len(msg) > 12):
                    msg = input("Digitare un messaggio con meno di 12 caratteri: \n")
                
                baseTopic = baseTopic +"display"
                Home.Publish(baseTopic, msg, "display")

    print("Finito!\n")



