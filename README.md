# scf-prototipo
Prototipo para la Práctica de Sistemas Ciberfísicos, del Máster Universitario en Ingeniería Informática de la ESI de Ciudad Real.

# Pasos de instalación

## 1. Pull Docker Images

docker pull telegraf

docker pull carfer10/prototipo-scf:influxdbcustom-0.1

docker pull carfer10/prototipo-scf:grafanacustom-0.1

docker pull eclipse-mosquitto

docker pull nodered/node-red

## 2. Despliegue Arquitectura

Abrimos una consola situándonos en el directorio raíz de este repositorio y ejecutamos los siguientes comandos:

docker network create my_bridge

docker run -p 8086:8086 --name=influxdb --net=my_bridge carfer10/prototipo-scf:influxdbcustom-0.1

curl -i http://localhost:8086/query --data-urlencode "q=CREATE DATABASE prototipo"

curl -i 'http://localhost:8086/write?db=prototipo' --data-binary 'nodoIoT,dniCliente="1111111A",nombre="Fernando",apellidos="Vallejo",temperatura=20 calidadAire=20'

docker run -p 2883:1883 --name=broker --net=my_bridge eclipse-mosquitto

docker run -v <Direccion absoluta de raíz del repositorio>/confs/telegraf.conf:/etc/telegraf/telegraf.conf --net=my_bridge --name=telegraf telegraf

docker run -p 3000:3000 --name=grafana --net=my_bridge carfer10/prototipo-scf:grafanacustom-0.1

## 3. Despliegue Node Red

Para poder visualizar un modelo con Node Red que se conecte a nuestro MQTT Broker realizamos los siguientes pasos:

docker run -it -p 1880:1880 -v node_red_data:/data --name mynodered nodered/node-red

Una vez desplegado node-red vamos a un navegador y entramos en la dirección: localhost:1880, a continuación, realizamos las siguientes acciones:

- Hacemos clic en el icono de Opciones en la esquina superior derecha
- Damos clic en "Import"
- Damos clic en "select a file to import" y buscamos el archivo "flows.json" que se encuentra en la raíz de nuestro repositorio
- Damos doble clic en el nodo "mqtt in" del nuevo flujo importado y modificamos la IP del servidor broker MQTT por la IP de nuestro equipo local
- A continuación, damos clic al botón "Deploy" para desplegar nuestro flujo de node-red
- Si nos situamos en la dirección: localhost:1880/ui podremos ver ciertos gráficos que empezarán a funcionar una vez carguemos el código en nuestro micro

## 4. Ejecución nodo IoT

Abrimos el proyecto de PlatformIO situado en el directorio "project" en Visual Studio code, abrimos el archivo "main.cpp" y modificamos las siguientes lineas:

- 21 #define TOPIC "1111111A/1/1/1" -> El topic MQTT está representado de la forma <DNI Usuario>/<ID Inmueble>/<ID Habitación>/<ID dispositivo IoT>, modificar de acuerdo a si se quiere instalar en otra habitación otro dispositivo, para este ejemplo son válidos: "1111111A/1/1/1" y "1111111A/1/2/1"
- 22 #define BROKER_IP "192.168.1.13" -> Sustituimos esta IP por la IP de nuestro equipo local
- 32/33 const char \*ssid = "MiFibra-36E9-plus"; const char \*password = "XMe3cpbk"; -> Sustituimos por la identificación y credenciales de nuestro Gateway (router WiFi)

Una vez sustituidas estas lineas presionamos las teclas Ctrl+Alt+U para cargar el código en nuestro dispositivo IoT (previamente conectado), si todo ha ido correctamente deberíamos ser capaces de ver como registra métricas en node-red como se indica en el paso anterior o en grafana:

- Vamos a la dirección localhost:3000
- Iniciamos sesión con el usuario admin y contraseña admin
- Nos dirigimos al Dashboard llamado "Dashboard" y podremos visualizar los datos
