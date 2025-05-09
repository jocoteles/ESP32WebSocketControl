# ESP32 WebSocket Control & Data Streaming

Um framework modular para comunicação WebSocket entre um ESP32 (atuando como Servidor WiFi Access Point) e um cliente web (HTML/JavaScript). Permite controle bidirecional de variáveis com validação e streaming de dados binários de alta frequência.

## Visão Geral

Este projeto fornece uma solução robusta e organizada para criar interações em tempo real entre um microcontrolador ESP32 e uma interface web. O ESP32 cria sua própria rede Wi-Fi (Access Point), e um cliente web (acessado de um dispositivo conectado a essa rede) se comunica com o ESP32 via WebSockets.

A comunicação suporta dois modos principais:

1.  **Controle de Variáveis (Get/Set via JSON):** O cliente pode solicitar (`get`) o valor atual de variáveis pré-definidas no ESP32 ou definir (`set`) novos valores. A biblioteca no ESP32 pode validar os limites (mínimo/máximo) ao definir valores numéricos. Suporta tipos `int`, `float` e `String`.
2.  **Streaming de Dados Binários:** O cliente pode comandar o ESP32 para iniciar ou parar um fluxo contínuo de dados binários. Ideal para dados de sensores de alta frequência, onde a sobrecarga do JSON é indesejável. Os dados são bufferizados no ESP32 e enviados em chunks para otimizar a transferência.

## Funcionalidades Principais

*   **Servidor WiFi Access Point:** ESP32 cria sua própria rede sem a necessidade de um roteador externo.
*   **IP Estático Configurável:** O IP do Access Point pode ser fixado para facilitar a conexão do cliente.
*   **Servidor WebSocket:** Comunicação bidirecional e de baixa latência usando o protocolo WebSocket.
*   **Biblioteca Modular (`ESP32WebSocketControl`):** Abstrai a complexidade da comunicação, permitindo que o código principal da aplicação (`.ino`) se concentre na lógica específica do hardware/sensores.
*   **Controle de Variáveis (JSON):**
    *   Comandos `get` e `set` padronizados.
    *   Suporte a tipos `int`, `float`, `String`.
    *   Validação opcional de limites (min/max) no ESP32.
*   **Streaming de Dados Binários:**
    *   Comandos `start_stream` e `stop_stream`.
    *   Callbacks na aplicação para iniciar/parar a lógica de aquisição.
    *   Envio eficiente de dados binários brutos (ex: leituras ADC).
    *   Bufferização no ESP32 para otimizar a taxa de transferência.
*   **Cliente Web Exemplo (`index.html`):**
    *   Interface de usuário limpa usando Pico.css.
    *   Controles para Get/Set de variáveis.
    *   Botões para iniciar/parar o streaming.
    *   Log para exibir dados binários recebidos.
    *   Conecta-se automaticamente ao IP estático definido.


## Requisitos

### Hardware

*   Placa de Desenvolvimento ESP32 (ex: ESP32-WROOM, ESP32-WROVER, ESP32-S2, ESP32-S3, etc.)

### Software

*   **Arduino IDE** (versão 1.8.19 ou superior) OU **PlatformIO** (com VS Code)
*   **Bibliotecas Externas (Instalar via Gerenciador de Bibliotecas Arduino/PlatformIO):**
    *   `ESP Async WebServer` por ESP32Async (certifique-se de instalar as dependências, incluindo `Async TCP` - também por ESP32Async)
    *   `ArduinoJson` por Benoit Blanchon (v6.x ou v7.x)
*   **Biblioteca Interna:**
    *   Os arquivos `ESP32WebSocketControl.h` e `ESP32WebSocketControl.cpp` *são* a biblioteca interna. Eles devem estar na mesma pasta que o arquivo `.ino` principal (`ApplicationDemo.ino`) para que o Arduino IDE/PlatformIO os encontre automaticamente.

## Instalação

1.  **Clonar ou Baixar:** Obtenha os arquivos deste repositório.
    ```bash
    git clone https://[URL_DO_SEU_REPOSITORIO]/ESP32WebSocketControl.git
    cd ESP32WebSocketControl
    ```
2.  **Instalar Bibliotecas Externas:**
    *   Abra o Arduino IDE.
    *   Vá em `Sketch` > `Include Library` > `Manage Libraries...`.
    *   Procure por `ESP Async WebServer` e instale-a (ela deve instalar `AsyncTCP` como dependência).
    *   Procure por `ArduinoJson` e instale-a.
    *   *(Se usar PlatformIO, adicione as dependências ao seu arquivo `platformio.ini`)*.
3.  **Abrir o Projeto:**
    *   Abra o Arduino IDE.
    *   Vá em `File` > `Open...` e navegue até a pasta `ApplicationDemo`, selecionando o arquivo `ApplicationDemo.ino`.
4.  **Configurar a Placa:**
    *   No Arduino IDE, vá em `Tools` > `Board` e selecione a placa ESP32 específica que você está usando.
    *   Certifique-se de que a porta COM correta está selecionada em `Tools` > `Port`.
5.  **Compilar e Carregar:**
    *   Clique no botão `Upload` (seta para a direita) no Arduino IDE para compilar o código e carregá-lo na sua placa ESP32.

## Configuração (Opcional)

Antes de carregar, você pode querer ajustar algumas configurações:

*   **Credenciais WiFi AP:** Em `ApplicationDemo.ino`, modifique `WIFI_SSID` e `WIFI_PASSWORD` se desejar nomes/senhas diferentes para a rede criada pelo ESP32.
*   **IP Estático:** O IP padrão do AP está fixado em `192.168.5.1` dentro da função `initWiFiWebSocketServer` em `ESP32WebSocketControl.cpp`. Se precisar alterá-lo, modifique-o lá **E** atualize a constante `ESP32_STATIC_IP` no arquivo `index.html`.
*   **Variáveis Configuráveis:** Adicione, remova ou modifique as entradas no array `configurableVariables` em `ApplicationDemo.ino` para definir as variáveis que seu projeto específico precisa controlar via JSON.
*   **Lógica de Streaming:**
    *   Ajuste os pinos analógicos (`ANALOG_PIN_x`) em `ApplicationDemo.ino`.
    *   Modifique `SAMPLES_PER_CHUNK` e `SAMPLE_INTERVAL_US` para ajustar a taxa e o tamanho do buffer de streaming.
    *   Adapte a estrutura `SensorDataPacket` se precisar enviar dados diferentes.
    *   Altere a lógica dentro da seção `if (isAppStreaming)` no `loop()` de `ApplicationDemo.ino` para ler seus sensores específicos ou gerar os dados que deseja transmitir.

## Como Usar e Testar

1.  **Carregue o Código:** Siga os passos de instalação para carregar `ApplicationDemo.ino` no seu ESP32.
2.  **Monitore a Saída Serial:** Abra o Monitor Serial no Arduino IDE (configure para 115200 baud). Observe as mensagens de inicialização. Ele deve confirmar o início do AP e exibir o endereço IP (ex: `--> ESP32 Access Point IP Address: 192.168.5.1`).
3.  **Conecte à Rede ESP32:** Use seu computador ou smartphone para procurar redes Wi-Fi. Conecte-se à rede com o SSID configurado (padrão: `ESP32_Control_AP`) usando a senha configurada (padrão: `password123`).
4.  **Abra o Cliente Web:** No navegador do dispositivo que você conectou à rede do ESP32, abra o arquivo `index.html` (você pode simplesmente arrastar o arquivo para a janela do navegador ou servi-lo localmente se preferir).
5.  **Verifique a Conexão:** A página deve carregar e o status no topo deve mudar de "Conectando..." para "Conectado ao ESP32" (em verde).
6.  **Teste Variáveis Get/Set:**
    *   Clique nos botões "Get" na tabela de variáveis. Os valores atuais devem ser preenchidos.
    *   Digite um novo valor no campo de input de uma variável (respeitando os limites, se houver).
    *   Clique no botão "Set" correspondente. O valor na tabela deve atualizar se o ESP32 aceitar a mudança.
7.  **Teste o Streaming:**
    *   Clique no botão "Iniciar Stream". O status do stream deve mudar para "ativo" e o log binário deve começar a exibir os dados recebidos (formatados como `C[chunk] P[pacote]: [leituras] @ [tempo]ms`).
    *   Clique no botão "Parar Stream". O log deve parar de receber novos dados e o status deve voltar para "parado".
8.  **Observe o Serial Monitor:** O monitor serial do ESP32 também exibirá logs sobre conexões de clientes, comandos recebidos e status do stream.

## Como Funciona (Resumido)

1.  O ESP32 (`ApplicationDemo.ino` + `ESP32WebSocketControl`) inicializa, configura um IP estático, cria um Access Point WiFi e inicia um servidor Web/WebSocket.
2.  Um dispositivo cliente conecta-se à rede WiFi do ESP32.
3.  O cliente abre o `index.html` no navegador.
4.  O JavaScript em `index.html` estabelece uma conexão WebSocket com `ws://192.168.5.1/ws`.
5.  **Comandos JSON:** O cliente envia mensagens JSON (ex: `{"action": "get", "variable": "led_intensity"}`) via WebSocket. A biblioteca no ESP32 as recebe, processa (`get`, `set`, `start_stream`, `stop_stream`), interage com as variáveis ou chama os callbacks da aplicação, e envia respostas JSON (valores ou status) de volta.
6.  **Stream Binário:** Quando `start_stream` é recebido, o `loop()` no `.ino` começa a ler os sensores, preencher o `sensorDataBuffer`, e chama `broadcastBinaryData()` quando o buffer está cheio. A biblioteca envia esses dados binários brutos a todos os clientes conectados. O JavaScript no cliente recebe o `ArrayBuffer`, decodifica-o usando `DataView` e exibe/processa os dados.

## Customização

Para adaptar este framework ao seu próprio projeto:

1.  **Modifique `ApplicationDemo.ino`:**
    *   Renomeie o arquivo se desejar.
    *   Ajuste as configurações de WiFi e pinos.
    *   Defina suas próprias variáveis em `configurableVariables`.
    *   Implemente sua lógica de leitura de sensores/atuadores dentro do `loop()` e, se usar streaming, dentro da condição `if (isAppStreaming)`.
    *   Adapte a estrutura `SensorDataPacket` se o formato do seu stream for diferente.
    *   Modifique os callbacks `application_onStreamStart`/`Stop` se precisar de ações específicas ao iniciar/parar o stream (ex: ligar/desligar sensores).
2.  **Modifique `index.html`:**
    *   Altere a interface do usuário para corresponder às suas variáveis e necessidades de visualização/controle.
    *   Se mudar a `SensorDataPacket`, ajuste as constantes `SENSOR_PACKET_BYTES`, `SENSORS_PER_PACKET` e a lógica de decodificação em `processBinaryData`.
    *   Adicione gráficos ou outros elementos de visualização para os dados do stream.
3.  **(Avançado) Modifique `ESP32WebSocketControl.h/.cpp`:** Apenas se precisar alterar a lógica fundamental da comunicação (ex: adicionar novos tipos de ação JSON, mudar o endpoint WebSocket, etc.).


## Authoring

This project was strongly developed with Gemini 2.5 Pro AI Model.

---