#-*- coding: UTF-8 -*-

import ssl
import urllib3
from bs4 import BeautifulSoup
import biplist
import os
import requests
import json
import threading
import shutil

BASEPATH = "https://opensource.apple.com/tarballs/"
CONFIGPATH = "download/config/config.json"


def downloadFile(config = None,name = ""):
    url = config["downloadUrl"]
    try:
        request = requests.get(url)
        file = open(name, "wb")
        file.write(request.content)
        config["download"] = 1
    except Exception as result:
        print(result)


def mkdirIfNotExist(path=""):
    if not os.path.exists(path):
        os.makedirs(path)
    return path


def perserFileDownload(config = None):
    if config["download"] == 1 :
        return
    url = config["downloadUrl"]
    print(url + " downloading")
    try:
        response = requests.get(url)
        file = open(config["filePath"]+"/"+config["name"], "wb")
        file.write(response.content)
        config["download"] = 1
        dataSize = response.headers["Content-Length"]
        print(config["downloadUrl"] + " download finish" + " " + str(int((int(dataSize))/1024)+1) + "KB")
    except Exception as result:
        print("download fail : "+str(result))

def perserDirDownload(config = None) :
    threads = []
    for obj in config :
        if obj["attr"] == "[GZ]":
            mkdirIfNotExist(obj["filePath"])
            t = threading.Thread(target=perserFileDownload, args=(obj,))
            threads.append(t)
            t.start()
            #if "WebCore" in obj["filePath"] :
            #    t.join()
    print("开启线程个数"+str(len(threads)))
    for t in threads :
        t.join()


def perserConfigToDownload(config = None):
    for obj in config :
        if obj["attr"] == "[DIR]" :
            name = obj["name"]
            print(name + " dir is downloading ...")
            perserDirDownload(obj["list"])
            print(name + " dir is download finish")
            open(CONFIGPATH,"wb+").write(json.dumps(config).encode())
        else:
            t = threading.Thread(target=perserFileDownload,args=(obj,))
            t.start()
            t.join()

def getDownloadConfig(url="",config = None,downloadPath = "",unzipPath = ""):
    # request xml data
    print(url)
    http = urllib3.PoolManager(num_pools=5)
    response = http.request('GET',url)
    # parser xml
    soup = BeautifulSoup(response.data.decode(), "lxml")
    tdList = soup.select("td[valign]")

    for td in tdList:
        next_element = td.next_element
        td_name = next_element.attrs["href"]

        next_element = next_element.next_element
        td_attr = next_element.attrs["alt"]

        if td_name == "./../" or td_name == "./..":
            print(td_name, "    ", td_attr, "    ", "ignore")

        elif td_attr == "[DIR]" or td_attr == "[CONFIG]" or td_attr == "[SH]":
            print(td_name, "    ", td_attr, "    ", "go to next level")
            dirName = td_name[:len(td_name)-1]
            dir_attr = {"name":dirName,
                        "attr":"[DIR]",
                        "list":[]}
            config.append(dir_attr)

            getDownloadConfig(url + td_name,dir_attr["list"],str(downloadPath+"/"+dirName),str(unzipPath+"/"+dirName))
        elif td_attr == "[GZ]":
            print(td_name, "    ", td_attr, "    ", "go to download file")
            file_attr = {
                "downloadUrl":url+td_name,
                "download":0,
                "unzip":0,
                "attr":"[GZ]",
                "name":td_name,
                "filePath":str(downloadPath),
                "unzipPath":str(unzipPath)}
            config.append(file_attr)
        else:
            print("error ---------------------", td_name, td_attr)


def unzip(config = None):
    for file in config:
        if file["unzip"] == 0:
            try:
                shutil.unpack_archive(file["filePath"]+"/"+file["name"], file["unzipPath"])
                file["unzip"] = 1
                print(file["name"]+" unzip finish ")
            except Exception as error:
                if "[Errno 22] Invalid argument" in str(error):
                    file["unzip"] = 1
                    print(file["name"] + " unzip finish ")
                else:
                    file["download"] = 0
                    print(file["name"] +" unzip fail " + str(error))
        else:
            print(file["name"] + " already unzip ")


def unzipDir(config = None):
    for path in config:
        if path["attr"] == "[DIR]":
            unzip(path["list"])
            open(CONFIGPATH, "wb+").write(json.dumps(config).encode())
            print(path["name"] + " dir is unzip finish")

config = json.load(open(CONFIGPATH))
unzipDir(config)

exit(0)

if os.path.exists(CONFIGPATH):
    config = json.load(open(CONFIGPATH))
    perserConfigToDownload(config)
else:
    config = []
    getDownloadConfig(BASEPATH, config, "download/gzip", "download/unzip")
    configDir = "download/config/"
    if not os.path.exists(configDir):
        os.makedirs(configDir)
    open(CONFIGPATH, "wb+").write(json.dumps(config).encode())

    print("download config is exist, download ...")
    config = json.load(open("CONFIGPATH"))
    perserConfigToDownload(config)

