import paho.mqtt.client as mqtt
import sqlite3
import json
import threading
from flask import Flask, jsonify
from flask_cors import CORS # Import CORS

# --- CONFIGURAÇÕES ---
MQTT_BROKER = "5e872d89b8ad4f9f8b879af366b22c4a.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "Avila"
MQTT_PASSWORD = "Adimin123"
# Assina todos os tópicos dentro de 'sdap/'. O '#' é um coringa.
MQTT_TOPICS = "sdap/#" 
DATABASE_FILE = "sdap_database.py.db"

# --- BANCO DE DADOS ---
def init_db():
    """Cria as tabelas no banco de dados se elas não existirem."""
    conn = sqlite3.connect(DATABASE_FILE)
    cursor = conn.cursor()
    # Cria uma tabela para cada tipo de dado
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS temperatura (
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            valor REAL
        )
    ''')
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS som (
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            valor_raw INTEGER
        )
    ''')
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS status (
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            atividade_paranormal INTEGER,
            estado TEXT
        )
    ''')
    conn.commit()
    conn.close()
    print("Banco de dados inicializado.")

# --- LÓGICA DO CLIENTE MQTT ---
def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("Conectado ao Broker MQTT com sucesso!")
        client.subscribe(MQTT_TOPICS)
        print(f"Assinando os tópicos: {MQTT_TOPICS}")
    else:
        print(f"Falha ao conectar, código de erro: {rc}")

def on_message(client, userdata, msg):
    """Função chamada sempre que uma mensagem chega."""
    topic = msg.topic
    payload = msg.payload.decode('utf-8')
    print(f"Recebido -> Tópico: {topic} | Mensagem: {payload}")
    
    try:
        data = json.loads(payload)
        conn = sqlite3.connect(DATABASE_FILE)
        cursor = conn.cursor()

        if topic == "sdap/sensor/temperatura":
            cursor.execute("INSERT INTO temperatura (valor) VALUES (?)", (data['valor'],))
        elif topic == "sdap/sensor/som":
            cursor.execute("INSERT INTO som (valor_raw) VALUES (?)", (data['valor_raw'],))
        elif topic == "sdap/dispositivo/status":
            cursor.execute("INSERT INTO status (atividade_paranormal, estado) VALUES (?, ?)", 
                           (data['atividade_paranormal'], data['estado']))
        
        conn.commit()
        conn.close()
    except Exception as e:
        print(f"Erro ao processar mensagem ou salvar no banco: {e}")

def start_mqtt_client():
    """Inicia e mantém o cliente MQTT rodando em uma thread separada."""
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    client.tls_set(tls_version=mqtt.ssl.PROTOCOL_TLS) # Habilita a conexão segura TLS
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_forever() # Mantém o cliente rodando em segundo plano

# --- LÓGICA DA API (FLASK) ---
app = Flask(__name__)
CORS(app) # Habilita CORS para permitir que o navegador acesse a API

@app.route('/dados/<sensor>')
def get_sensor_data(sensor):
    """Endpoint da API para buscar o último dado de um sensor."""
    conn = sqlite3.connect(DATABASE_FILE)
    conn.row_factory = sqlite3.Row # Permite acessar colunas por nome
    cursor = conn.cursor()
    
    # Valida o sensor para evitar injeção de SQL
    if sensor not in ['temperatura', 'som', 'status']:
        return jsonify({"error": "Sensor inválido"}), 404
        
    query = f"SELECT * FROM {sensor} ORDER BY timestamp DESC LIMIT 1"
    cursor.execute(query)
    data = cursor.fetchone()
    conn.close()
    
    if data:
        # Converte o resultado para um dicionário e retorna como JSON
        return jsonify(dict(data))
    else:
        return jsonify({}), 404

# --- INICIALIZAÇÃO ---
if __name__ == '__main__':
    init_db() # Garante que o banco e as tabelas existam
    
    # Inicia o cliente MQTT em uma nova thread para não bloquear o servidor web
    mqtt_thread = threading.Thread(target=start_mqtt_client)
    mqtt_thread.daemon = True
    mqtt_thread.start()
    
    # Inicia o servidor da API Flask
    # O host '0.0.0.0' permite acesso de outros dispositivos na mesma rede
    app.run(host='0.0.0.0', port=5000)