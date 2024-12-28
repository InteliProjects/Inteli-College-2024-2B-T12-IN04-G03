// Importação de bibliotecas necessárias
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include "UbidotsEsp32Mqtt.h"

// Definição das constantes necessárias para conexão com o Ubidots
const char *TOKEN_UBIDOTS_G03 = "BBUS-nQhubeoMQ6ywNUlkcqtCTRno5uqSVo"; // Token do grupo no Ubidots
const char *SSID_WIFI = "Inteli.Iot"; // Nome da rede Wi-Fi
const char *SENHA_WIFI = "@Intelix10T#"; // Senha da rede Wi-Fi
const char *DISPOSITIVO = "sensor_prensa_g03"; // Label do dispositivo no Ubidots
const char *DISTANCIA = "distancia_g03"; // Label da variável para a distância no Ubidots
const char *ESTADO_PRENSA = "estado_prensa_g03"; // Label da variável para o estado da prensa no Ubidots
const char *ALERTA_TROCA_OLEO = "precisa_lubrificar_prensa_g03"; // Label da variável para o alerta de troca de óleo necessária no Ubidots
const char *BOTAO_UBIDOTS = "botao_registra_lubrificacao_prensa_g03"; // Label da variável para o botão de troca de óleo no Ubidots
const char *ID_CLIENTE = "carbone"; // ID do usuário (crie um)

// Variável global para monitorar o estado do botão no Ubidots
bool botaoUbidots = false; // Estado inicial do botão de troca de óleo

// Instância para o display LCD I2C com endereço 0x27 e dimensões 16x2
LiquidCrystal_I2C displayLCD(0x27, 16, 2);

// Função de callback necessária para receber variáveis do Ubidots
void _callback(char *topic, byte *payload, unsigned int length) {
  // Converte o payload recebido para inteiro e atualiza o estado do botão
  int state = atoi((char *)payload);
  botaoUbidots = (state == 1); // Define verdadeiro se o valor recebido for 1

  // Exibe o estado do botão no monitor serial
  Serial.println(botaoUbidots ? "Botão Ubidots pressionado" : "Botão Ubidots desativado");
}

// Classe responsável pela conexão e comunicação com o Ubidots
class ConectorUbidots {
private:
  // Instância da biblioteca Ubidots com o token e ID do cliente
  Ubidots ubidots = Ubidots(TOKEN_UBIDOTS_G03, ID_CLIENTE);

public:
  // Função para inicializar a conexão com o Ubidots
  void iniciar() {
    ubidots.connectToWifi(SSID_WIFI, SENHA_WIFI); // Conexão com a rede Wi-Fi
    ubidots.setCallback(_callback); // Define a função de callback
    ubidots.setup(); // Configuração inicial do Ubidots
    ubidots.reconnect(); // Garante a reconexão inicial
    ubidots.subscribeLastValue(DISPOSITIVO, BOTAO_UBIDOTS); // Subscreve a variável do botão no Ubidots
  }

  // Função para garantir que o dispositivo permaneça conectado ao Ubidots
  void conectar() {
    if (!ubidots.connected()) { // Verifica se a conexão foi perdida
      ubidots.reconnect(); // Reconecta ao Ubidots
      ubidots.subscribeLastValue(DISPOSITIVO, BOTAO_UBIDOTS); // Reinscreve a variável do botão
    }
    ubidots.loop(); // Mantém a conexão ativa
  }

  // Publica um valor específico para uma variável no Ubidots
  void publicar(const char *labelVariavel, float valorVariavel) {
    ubidots.add(labelVariavel, valorVariavel); // Adiciona a variável e seu valor
    ubidots.publish(DISPOSITIVO); // Publica os dados no dispositivo configurado
  }
};

// Classe para gerenciamento do sensor de distância ultrassônico
class SensorDistancia {
private:
  // Pinos para o Echo e Trigger do sensor
  int8_t _pinEcho;
  int8_t _pinTrig;

public:
  // Construtor da classe, inicializa os pinos Echo e Trigger
  SensorDistancia(int8_t pinEcho, int8_t pinTrig) : _pinEcho(pinEcho), _pinTrig(pinTrig) {
    pinMode(_pinTrig, OUTPUT); // Define o pino Trigger como saída
    pinMode(_pinEcho, INPUT);  // Define o pino Echo como entrada
  }

  // Função para medir a distância utilizando o sensor ultrassônico
  float medirDistancia() {
    digitalWrite(_pinTrig, LOW); // Garante que o Trigger inicie em LOW
    delayMicroseconds(2);
    digitalWrite(_pinTrig, HIGH); // Sinal de pulso para ativar o sensor
    delayMicroseconds(10);
    digitalWrite(_pinTrig, LOW); // Retorna o Trigger para LOW

    // Mede o tempo de retorno do sinal (pulso recebido no Echo)
    unsigned long duracao = pulseIn(_pinEcho, HIGH);

    // Calcula a distância em centímetros (tempo * velocidade do som / 2)
    unsigned long distancia = duracao * 0.034 / 2;

    return distancia; // Retorna a distância medida
  }
};

// Classe responsável pelo controle do display LCD
class DisplayLCD {
public:
  // Inicializa o display com uma mensagem inicial
  void iniciar() {
    displayLCD.init(); // Inicializa a comunicação com o display
    displayLCD.backlight(); // Ativa a luz de fundo do display
    displayLCD.setCursor(0, 0); // Define a posição inicial do cursor
    displayLCD.print("Iniciando..."); // Exibe mensagem inicial no display
  }

  // Exibe a distância medida do pistão da prensa no display em centímetros
  void exibirDistancia(float distancia) {
    displayLCD.clear(); // Limpa o display
    displayLCD.setCursor(0, 0); // Posiciona o cursor na linha 0, coluna 0
    displayLCD.print("Dist prensa: "); // Exibe o título da informação
    displayLCD.setCursor(4, 1); // Posiciona o cursor na linha 1, coluna 4
    displayLCD.print(distancia); // Exibe a distância medida
    displayLCD.print(" cm"); // Adiciona a unidade de medida
  }

  // Exibe o tempo de uso do óleo no display em segundos
  void exibirTempoOleo(unsigned long tempoUsoOleo) {
    displayLCD.clear(); // Limpa o display
    displayLCD.setCursor(0, 0); // Posiciona o cursor na linha 0, coluna 0
    displayLCD.print("Tempo Uso Oleo: "); // Exibe o título da informação
    displayLCD.setCursor(4, 1); // Posiciona o cursor na linha 1, coluna 4
    displayLCD.print(tempoUsoOleo / 1000); // Converte o tempo (ms) para segundos e exibe
    displayLCD.print(" s"); // Adiciona a unidade de medida
  }

  // Exibe a confirmação de que a troca de óleo foi realizada
  void exibirTrocaOleo() {
    displayLCD.clear(); // Limpa o display
    displayLCD.setCursor(0, 0); // Posiciona o cursor na linha 0, coluna 0
    displayLCD.print("Troca de Óleo"); // Exibe a mensagem principal
    displayLCD.setCursor(0, 1); // Posiciona o cursor na linha 1, coluna 0
    displayLCD.print("Realizada!"); // Exibe a confirmação da ação
  }

  // Exibe uma mensagem de erro no sensor no display
  void exibirErro() {
    displayLCD.clear(); // Limpa o display
    displayLCD.setCursor(0, 1); // Posiciona o cursor na linha 1, coluna 0
    displayLCD.print("Erro no sensor!"); // Exibe a mensagem de erro
  }
};

// Classe do dispositivo IoT responsável pela lógica principal do sistema
class DispositivoPrensa {
private:
  // Instâncias dos componentes utilizados
  SensorDistancia sensor; // Instância do sensor de distância
  ConectorUbidots ubidots; // Instância do conector Ubidots
  DisplayLCD display; // Instância do display LCD

  // Pinos físicos conectados aos LEDs e botão
  uint8_t _pinLEDVerde, _pinLEDVermelho, _pinLEDAmarelo, _pinBotao;

  // Variáveis de tempo para controle de ciclos e verificações
  unsigned long _tempoInicial, _tempoUsoOleo, _tempoUltimaAtualizacaoLCD, _tempoUltimaExibicaoDisplay, _tempoTotal;
  const unsigned long _tempoLimiteUsoOleo = 30000; // Tempo limite de uso do óleo (em ms)
  const unsigned long _tempoExibicaoLCD = 2000; // Intervalo de atualização do display (em ms)
  bool _mostrarTempoUsoOleo = true; // Flag para alternar exibição do tempo de uso do óleo
  bool _estadoPrensa = true; // Estado atual da prensa (ligada/desligada)

  // Variáveis auxiliares para controle de medições
  unsigned long tempoUltimaMedidaDistancia = 0; // Armazena o tempo da última medição
  unsigned long tempoUltimaVerificacao = 0; // Armazena o tempo da última verificação do estado
  float ultimaDistancia = 0; // Última distância medida

  // Atualiza o estado dos LEDs com base no funcionamento da prensa e no uso do óleo
  void _atualizarLEDs() {
    digitalWrite(_pinLEDVermelho, !_estadoPrensa); // Acende o LED vermelho se a prensa estiver desligada
    digitalWrite(_pinLEDVerde, _estadoPrensa); // Acende o LED verde se a prensa estiver ligada
    digitalWrite(_pinLEDAmarelo, _tempoUsoOleo >= _tempoLimiteUsoOleo); // Acende o LED amarelo se o uso do óleo ultrapassar o limite
  }

  // Calcula o tempo de uso do óleo enquanto a prensa está ligada
  void _calcularTempoUsoOleo() {
    if (_estadoPrensa) {
      _tempoUsoOleo = millis() - _tempoInicial; // Tempo atual menos o tempo inicial
    }
  }

  // Calcula o tempo total em que a prensa permaneceu ligada
  void _calcularTempoTotal() {
    static unsigned long _ultimoRegistroTempo = 0; // Armazena o tempo da última verificação
    if (_estadoPrensa) {
      unsigned long agora = millis(); // Tempo atual
      _tempoTotal += (agora - _ultimoRegistroTempo); // Adiciona o tempo decorrido ao total
    }
    _ultimoRegistroTempo = millis(); // Atualiza o tempo da última verificação
  }

  // Registra a troca de óleo e reseta os contadores necessários
  void _registrarTrocaOleo() {
    _tempoInicial = millis(); // Atualiza o tempo inicial de uso do óleo
    _tempoUsoOleo = 0; // Reseta o tempo de uso do óleo
    botaoUbidots = false; // Reseta o estado do botão
    display.exibirTrocaOleo(); // Exibe a mensagem de confirmação no display
  }

  // Verifica o estado do botão físico e virtual, acionando a troca de óleo
  void _verificarBotao() {
    if (digitalRead(_pinBotao) == HIGH || botaoUbidots) { // Verifica se o botão físico ou o botão virtual foi pressionado
      _registrarTrocaOleo(); // Registra a troca de óleo
    }
  }

  // Verifica o estado da prensa com base na movimentação do pistão
  void _verificarEstadoPrensa() {
      static const float LIMIAR_MOVIMENTO = 2.0; // Limiar de movimento em cm
      static const int NUM_AMOSTRAS_MEDIA = 5;  // Número de amostras para a média móvel
      static float ultimasMedidas[NUM_AMOSTRAS_MEDIA] = {0}; // Histórico de medidas
      static int indiceMedidaAtual = 0; // Índice para armazenar as novas medidas

      unsigned long tempoAtual = millis(); // Captura o tempo atual
      float distanciaAtual = sensor.medirDistancia(); // Realiza a medição da distância atual

      // Atualiza a janela de média móvel
      ultimasMedidas[indiceMedidaAtual] = distanciaAtual;
      indiceMedidaAtual = (indiceMedidaAtual + 1) % NUM_AMOSTRAS_MEDIA; // Atualiza índice circular

      // Calcula a média das últimas medidas
      float soma = 0;
      for (int i = 0; i < NUM_AMOSTRAS_MEDIA; i++) {
          soma += ultimasMedidas[i];
      }
      float mediaDistancia = soma / NUM_AMOSTRAS_MEDIA;

      // Verifica se houve movimentação significativa
      if (_estadoPrensa) {
          // Desliga se a distância média permanecer inalterada por tempo prolongado
          if (fabs(mediaDistancia - ultimaDistancia) < LIMIAR_MOVIMENTO &&
              tempoAtual - tempoUltimaVerificacao >= 30000) {
              tempoUltimaVerificacao = tempoAtual;
              _estadoPrensa = false; // Desliga a prensa por inatividade
          }
      } else {
          // Religa se houver movimentação consistente do pistão
          if (fabs(mediaDistancia - ultimaDistancia) >= LIMIAR_MOVIMENTO) {
              _estadoPrensa = true; // Religa a prensa
          }
      }

      ultimaDistancia = mediaDistancia; // Atualiza a última distância média
  }

public:
  // Construtor da classe DispositivoPrensa
  // Inicializa os pinos dos sensores, LEDs e botão, além de variáveis internas
  DispositivoPrensa(int pinEcho, int pinTrig, uint8_t pinLEDVerde, uint8_t pinLEDVermelho, uint8_t pinLEDAmarelo, uint8_t pinBotao)
    : sensor(pinEcho, pinTrig), // Inicializa o sensor de distância com os pinos Echo e Trig
      _pinLEDVerde(pinLEDVerde), _pinLEDVermelho(pinLEDVermelho),
      _pinLEDAmarelo(pinLEDAmarelo), _pinBotao(pinBotao),
      _estadoPrensa(false), _tempoUsoOleo(0), _tempoTotal(0) {} // Inicializa variáveis internas

  // Inicializa o sistema configurando pinos e iniciando componentes
  void iniciar() {
    pinMode(_pinLEDVerde, OUTPUT); // Configura pino do LED verde como saída
    pinMode(_pinLEDVermelho, OUTPUT); // Configura pino do LED vermelho como saída
    pinMode(_pinLEDAmarelo, OUTPUT); // Configura pino do LED amarelo como saída
    pinMode(_pinBotao, INPUT_PULLUP); // Configura o botão como entrada com pull-up interno

    Serial.begin(115200); // Inicia a comunicação serial com velocidade de 115200 bps

    display.iniciar(); // Inicializa o display LCD
    ubidots.iniciar(); // Inicializa a conexão com a plataforma Ubidots
    _tempoInicial = millis(); // Salva o tempo inicial do sistema
  }

  // Mantém o sistema em operação, atualizando sensores, LEDs e display
  void operar() {
    unsigned long tempoAtual = millis(); // Captura o tempo atual
    float distancia = sensor.medirDistancia(); // Mede a distância atual do pistão

    ubidots.conectar(); // Garante que o dispositivo está conectado à Ubidots

    // Alterna entre exibir tempo de uso do óleo e distância no display
    if (tempoAtual - _tempoUltimaAtualizacaoLCD >= _tempoExibicaoLCD) {
      _tempoUltimaAtualizacaoLCD = tempoAtual;

      if (tempoAtual - _tempoUltimaExibicaoDisplay >= _tempoExibicaoLCD) {
        _tempoUltimaExibicaoDisplay = tempoAtual;

        if (_mostrarTempoUsoOleo) {
          display.exibirTempoOleo(_tempoUsoOleo); // Exibe tempo de uso do óleo
        } else {
          display.exibirDistancia(distancia); // Exibe distância medida
        }

        _mostrarTempoUsoOleo = !_mostrarTempoUsoOleo; // Alterna a variável para a próxima exibição
      }
    }

    // Publica as variáveis para a plataforma Ubidots
    ubidots.publicar(DISTANCIA, distancia); // Publica a distância medida
    ubidots.publicar(ESTADO_PRENSA, _estadoPrensa); // Publica o estado da prensa (ligada/desligada)

    // Publica alerta de troca de óleo caso o tempo de uso atinja o limite
    if (_tempoUsoOleo >= _tempoLimiteUsoOleo) {
      ubidots.publicar(ALERTA_TROCA_OLEO, 1); // Publica alerta de troca de óleo
    } else {
      ubidots.publicar(ALERTA_TROCA_OLEO, 0); // Publica valor normal (sem alerta)
    }

    _verificarEstadoPrensa(); // Verifica o estado da prensa e movimentação do pistão

    // Atualiza os contadores de tempo
    _calcularTempoUsoOleo(); // Calcula o tempo de uso do óleo
    _calcularTempoTotal(); // Calcula o tempo total em que a prensa esteve ligada

    _verificarBotao(); // Verifica se o botão físico ou virtual foi pressionado

    _atualizarLEDs(); // Atualiza o estado dos LEDs conforme a lógica definida

    // Exibe o tempo total de funcionamento da prensa no monitor serial
    Serial.print("Tempo total ligado: ");
    Serial.print(_tempoTotal / 1000); // Converte o tempo total para segundos
    Serial.println(" segundos");

    ubidots.publicar("tempo_total_operacao_g03", _tempoTotal / 1000);
    ubidots.publicar("tempo_uso_oleo_prensa", _tempoUsoOleo / 1000);
  }
};

// Instância da classe do dispositivo IoT com os respectivos pinos
DispositivoPrensa dispositivo(26, 33, 23, 19, 5, 12);

// Função de configuração do Arduino (executada uma vez no início)
void setup() {
  dispositivo.iniciar(); // Inicia o dispositivo e configura os componentes
}

// Função principal do Arduino (executada em loop)
void loop() {
  dispositivo.operar(); // Mantém o dispositivo em operação contínua
}
