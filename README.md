Sistema de Detecção de Anomalias Paranormais (SDAP)
Introdução
Inspirado em equipamentos popularizados por programas de investigação paranormal, como "Supernatural", este projeto aborda a necessidade de monitorar ambientes em busca de anomalias que possam indicar uma presença inexplicada. Investigadores muitas vezes precisam de ferramentas que possam ser deixadas em um local para coletar dados de forma autônoma e permitir o monitoramento remoto em tempo real.

O SDAP (Sistema de Detecção de Anomalias Paranormais) foi desenvolvido como uma solução de IoT (Internet das Coisas) para este cenário. O sistema utiliza um microcontrolador ESP32 conectado a sensores de temperatura e som para detectar duas das ocorrências mais comuns associadas a fenômenos paranormais: quedas bruscas de temperatura ("pontos frios") e ruídos inexplicados (Fenômenos de Voz Eletrônica - EVP).

Os dados coletados são processados em tempo real para gerar um "nível de atividade", que é exibido localmente através de LEDs. Simultaneamente, todas as leituras são transmitidas via protocolo MQTT para um sistema central, onde são armazenadas em um banco de dados e exibidas em um dashboard web, permitindo a análise histórica e o monitoramento remoto por parte do "investigador".

Objetivos do Sistema
O principal objetivo deste projeto é desenvolver um sistema IoT ponta-a-ponta para monitoramento ambiental. Os objetivos específicos são:

Desenvolver um dispositivo embarcado utilizando a plataforma ESP32.

Utilizar um sensor de temperatura (LM35) para detectar variações térmicas e um sensor de som (KY-037) para captar distúrbios sonoros.

Implementar um atuador (LED de Alerta Vermelho) que indica níveis críticos de atividade e pode ser controlado remotamente.

Transmitir os dados dos sensores de forma segura e em tempo real para um broker na nuvem utilizando o protocolo MQTT sobre TLS.

Implementar um serviço de backend em Python para receber os dados do MQTT, persisti-los em um banco de dados SQLite e expô-los através de uma API REST.

Criar uma aplicação frontend (dashboard web com HTML/JS) para se conectar à API, exibir os dados históricos e em tempo real.

Componentes e Tecnologias
Hardware
Microcontrolador: ESP32 DevKitC V4

Sensor de Temperatura: LM35 (Analógico)

Sensor de Som: Módulo KY-037 (Analógico)

Indicadores Visuais: 5 LEDs (2 verdes, 2 amarelos, 1 vermelho)

Resistores: 5x 220Ω (para os LEDs)

Protoboard e Jumpers

Software, Cloud e Protocolos
Firmware do Dispositivo: C++ (Arduino Framework) na Arduino IDE

Protocolo de Comunicação: MQTT sobre TLS (MQTTS)

Broker MQTT: HiveMQ Cloud (Instância privada e segura)

Backend: Script Python 3

Cliente MQTT: Biblioteca paho-mqtt

Servidor de API: Framework Flask

Banco de Dados: SQLite3

Frontend: HTML5, CSS3, JavaScript (sem frameworks)

Arquitetura e Funcionamento
O sistema opera em um fluxo contínuo de coleta, transmissão, armazenamento e visualização de dados.

Coleta (ESP32): A cada segundo, o ESP32 realiza a leitura dos sensores de temperatura (LM35) e som (KY-037).

Transmissão (MQTT): Imediatamente após a leitura, o ESP32 publica os dados brutos de cada sensor em tópicos MQTT distintos e seguros.

Processamento Local (ESP32): Com base nas leituras, o ESP32 calcula um "nível de atividade paranormal" (0 a 5). Este nível é exibido visualmente nos 5 LEDs da protoboard em tempo real. O nível de atividade também é publicado em um tópico de status.

Recepção e Persistência (Backend Python): Um script Python rodando em um servidor (ou computador local) está inscrito nos tópicos MQTT. Ao receber uma nova mensagem, ele a processa e insere a informação correspondente em uma tabela no banco de dados SQLite.

API (Backend Python): O mesmo script Python usa o framework Flask para criar endpoints HTTP (uma API). Esses endpoints, quando consultados, leem os dados mais recentes do banco de dados e os retornam em formato JSON.

Visualização (Frontend HTML): Uma página web (dashboard.html), aberta no navegador do usuário, faz requisições periódicas (a cada 3 segundos) para a API do backend Python, recebe os dados mais recentes e atualiza os elementos visuais (medidores, textos, etc.) no dashboard.

Fluxo MQTT
A comunicação MQTT é a espinha dorsal do sistema. A estrutura de tópicos foi desenhada para garantir a segregação dos dados, conforme os requisitos.

sdap/sensor/temperatura

Direção: ESP32 -> Backend

Propósito: Publica a leitura do sensor de temperatura.

Exemplo de Payload: {"unidade":"C", "valor":24.50}

sdap/sensor/som

Direção: ESP32 -> Backend

Propósito: Publica a leitura bruta do sensor de som.

Exemplo de Payload: {"valor_raw":720}

sdap/dispositivo/status

Direção: ESP32 -> Backend

Propósito: Publica o nível de atividade calculado e o estado geral do dispositivo.

Exemplo de Payload: {"atividade_paranormal":5, "estado":"ALERTA"}

sdap/atuador/alarme/comando

Direção: Cliente -> ESP32

Propósito: Permite que um cliente envie comandos para controlar o atuador (LED vermelho). O ESP32 está inscrito neste tópico.

Exemplo de Payload: "LIGAR" ou "DESLIGAR"

Estrutura do Código
Firmware do ESP32 (Arduino C++)
O loop principal gerencia a temporização, leitura, processamento e publicação.

C++

void loop() {
  // Garante a conexão com o MQTT e processa mensagens recebidas
  if (!mqtt.connected()) { /* ... reconexão ... */ }
  mqtt.loop();

  // Bloco principal que roda a cada segundo
  if (millis() - last_read_ms >= READ_INTERVAL_MS) {
    last_read_ms = millis();

    // 1. Leitura dos sensores
    float currentTemp = (analogRead(TEMP_SENSOR_PIN) / 4095.0) * 330.0;
    int soundLevel = analogRead(SOUND_SENSOR_PIN);

    // 2. Publica os dados brutos nos tópicos separados
    publishTemperature(currentTemp);
    publishSound(soundLevel);

    // 3. Calcula a atividade paranormal
    int activityLevel = map(soundLevel, 700, 4000, 0, 5);
    if (previousTemp > -50 && (previousTemp - currentTemp > TEMP_DROP_THRESHOLD)) {
      activityLevel += 2;
    }
    activityLevel = constrain(activityLevel, 0, 5);
    previousTemp = currentTemp;

    // 4. Atualiza os LEDs (atuador local)
    updateLeds(activityLevel);
    
    // 5. Publica o status geral
    publishStatus(activityLevel);
  }
}
Backend (Python com Flask e Paho-MQTT)
O backend tem duas partes principais: a que ouve o MQTT e a que serve a API.

Python

# Parte 1: Ouvinte MQTT que salva no banco de dados
def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode('utf-8')
    data = json.loads(payload)
    
    conn = sqlite3.connect(DATABASE_FILE)
    cursor = conn.cursor()

    if topic == "sdap/sensor/temperatura":
        cursor.execute("INSERT INTO temperatura (valor) VALUES (?)", (data['valor'],))
    # ... (lógica para outros tópicos) ...
    
    conn.commit()
    conn.close()

# Parte 2: Servidor de API que lê do banco de dados
app = Flask(__name__)
CORS(app)

@app.route('/dados/temperatura')
def get_sensor_data():
    conn = sqlite3.connect(DATABASE_FILE)
    # ... (código para consultar o banco) ...
    cursor.execute("SELECT * FROM temperatura ORDER BY timestamp DESC LIMIT 1")
    data = cursor.fetchone()
    conn.close()
    return jsonify(dict(data))
Guia de Instalação e Uso
Hardware: Monte o circuito na protoboard conforme os pinos definidos no código do ESP32 (TEMP_SENSOR_PIN=35, SOUND_SENSOR_PIN=34, ledPins[]=...).

Firmware (ESP32): Abra o código .ino na Arduino IDE, preencha suas credenciais de Wi-Fi e MQTT na seção de configuração, e carregue para a placa.

Backend (Python): Instale as dependências (pip install paho-mqtt Flask flask-cors) e, em um terminal, execute o comando python backend.py. Mantenha este terminal aberto.

Frontend: Abra o arquivo dashboard.html em qualquer navegador web moderno. Os dados começarão a ser exibidos e atualizados automaticamente.

Capturas de Tela do Sistema
Insira aqui as capturas de tela do seu projeto em funcionamento.

Exemplo 1: Imagem do circuito montado na protoboard.
[Imagem do seu hardware aqui]

Exemplo 2: Imagem do terminal rodando o backend Python, mostrando os logs de mensagens MQTT sendo recebidas.
[Print do seu terminal aqui]

Exemplo 3: Imagem do dashboard web final, exibindo os dados em tempo real com os medidores e gráficos.
[Print do seu dashboard aqui]

Considerações Finais
O desenvolvimento deste projeto foi uma jornada completa pelo ecossistema de IoT, desde o hardware de baixo nível até a visualização de dados em um frontend web.

Desafios Enfrentados:

Instabilidade do Hardware: O maior desafio foi diagnosticar o sensor de temperatura (LM35), que inicialmente apresentava leituras flutuantes e depois travadas em zero. O problema foi resolvido através de um processo metódico de isolamento (com um código de teste) e correção da fiação física.

Conflitos do ESP32: Durante a fase de projeto, foi importante pesquisar e selecionar os pinos corretos para leitura analógica (ADC1) para evitar o conhecido conflito com o subsistema de Wi-Fi, que inutiliza os pinos ADC2.

Configuração de Segurança: A conexão do frontend web com o broker HiveMQ Cloud via WebSockets seguros (WSS) falhou inicialmente devido à exigência de ALPN do broker, um detalhe técnico que precisou ser desabilitado no painel da HiveMQ para garantir a compatibilidade.

Configuração de Ambiente: Foram encontrados e superados erros comuns de configuração de ambiente de desenvolvimento, como a falta dos comandos npm e pip no PATH do sistema operacional.

Melhorias Possíveis:

Eficiência Energética: Implementar o modo Deep Sleep no ESP32 para que ele consuma muito menos energia, acordando apenas em intervalos definidos para enviar os dados, tornando o projeto viável para operação com baterias.

Sensores Adicionais: Adicionar um sensor de campo eletromagnético (EMF) real (como um magnetômetro) para tornar o dispositivo mais fiel à sua inspiração temática.

Dashboard Avançado: Utilizar uma biblioteca de gráficos como a Chart.js no frontend para exibir gráficos históricos mais ricos, consultando um endpoint de API que retorna um intervalo de tempo de dados, e não apenas o último.

Notificações em Tempo Real: No backend Python, adicionar uma lógica para enviar uma notificação (via Telegram, e-mail ou Push) sempre que o atividade_paranormal atingir o nível 5.
