// Importação das bibliotecas necessárias
#include <BME280I2C.h>
#include <Wire.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include "UbidotsEsp32Mqtt.h"

// Constantes para o endereço e registros do acelerômetro MMA8452
#define MMA8452_ADDRESS 0x1C
#define REG_CTRL_REG1 0x2A
#define REG_XYZ_DATA_CFG 0x0E
#define REG_OUT_X_MSB 0x01

// Configurações de informações da plataforma Ubidots
const char *TOKEN_UBIDOTS_G03 = "BBUS-nQhubeoMQ6ywNUlkcqtCTRno5uqSVo";  // Token do grupo
const char *SSID_WIFI = "Inteli.Iot";                                   // Nome da rede Wi-Fi
const char *SENHA_WIFI = "@Intelix10T#";                                // Senha da rede Wi-Fi
const char *DISPOSITIVO = "sensor_compressor_g03";                      // Rótulo do dispositivo
const char *TEMPO_LIGADO = "tempo_ligado";                              // Rótulo da variável para tempo ligado
const char *TEMPO_USO_OLEO = "tempo_uso_oleo";                          // Rótulo da variável para tempo ligado
const char *TEMPERATURA = "temperatura-1";                              // Rótulo da variável para temperatura
const char *ESTADO_COMPRESSOR = "estado_compressor";                    // Rótulo da variável para estado do compressor (ligado/desligado)
const char *ALERTA_TROCA_OLEO = "troca_oleo_necessaria";                // Rótulo da variável que alerta a necessidade de troca de óleo
const char *BOTAO_UBIDOTS = "registro_troca_oleo_g03";                      // Rótulo da ariável que indica se o óleo foi trocado
const char *ID_CLIENTE = "carbone";                                     // ID do usuário (crie um)

bool botaoUbidots = false;  // Variável para monitorar o estado do botão de troca de óleo no Ubidots (ligado/desligado)

// Instância para o display LCD I2C
LiquidCrystal_I2C displayLCD(0x27, 16, 2);

// Função de callback necessária para receber variáveis do Ubidots
void _callback(char *topic, byte *payload, unsigned int length) {
  int state = atoi((char *)payload);
  botaoUbidots = (state == 1);
  Serial.println(botaoUbidots ? "Botão Ubidots pressionado" : "Botão Ubidots desativado");
}

// Classe para conexão, recepção e publicação no Ubidots
class ConectorUbidots {
private:
  // Instância para a classe do Ubidots
  Ubidots ubidots = Ubidots(TOKEN_UBIDOTS_G03, ID_CLIENTE);

public:
  // Conecta ao Wi-Fi e inicializa o Ubidots
  void iniciar() {
    ubidots.connectToWifi(SSID_WIFI, SENHA_WIFI);
    ubidots.setCallback(_callback);
    ubidots.setup();
    ubidots.reconnect();
    ubidots.subscribeLastValue(DISPOSITIVO, BOTAO_UBIDOTS);
  }

  // Mantém a conexão entre o dispositivo e o Ubidots
  void conectar() {
    if (!ubidots.connected()) {
      ubidots.reconnect();
      ubidots.subscribeLastValue(DISPOSITIVO, BOTAO_UBIDOTS);
    }
    ubidots.loop();
  }

  // Publica um valor para uma variável registrada e rotulada no Ubidots
  void publicar(const char *labelVariavel, float valorVariavel) {
    ubidots.add(labelVariavel, valorVariavel);
    ubidots.publish(DISPOSITIVO);
  }
};

// Classe para gerenciar o sensor de vibração (acelerômetro)
class SensorVibracao {
private:
  uint8_t _pinSda, _pinScl;                                       // Pinos SDA e SCL do sensor
  float _ultimoValorX = 0, _ultimoValorY = 0, _ultimoValorZ = 0;  // Valores de aceleração mais recentes registrados em cada eixo
  unsigned long _ultimoRegistroVibracao = 0;                      // Momento da última leitura feita pelo sensor
  unsigned long _tempoLigadoInicio = 0;                           // Tempo em que o compressor foi ligado
  unsigned long _tempoTotalLigado = 0;                            // Tempo total que o compressor esteve ligado

  // Lê os dados do acelerômetro
  void _medirAceleracao(float &x, float &y, float &z) {
    Wire.beginTransmission(MMA8452_ADDRESS);
    Wire.write(REG_OUT_X_MSB);
    Wire.endTransmission(false);
    Wire.requestFrom(MMA8452_ADDRESS, 6);

    if (Wire.available() == 6) {
      int16_t xInicial = (Wire.read() << 8) | Wire.read();
      int16_t yInicial = (Wire.read() << 8) | Wire.read();
      int16_t zInicial = (Wire.read() << 8) | Wire.read();

      x = (float)xInicial / 4096;
      y = (float)yInicial / 4096;
      z = (float)zInicial / 4096;
    }
  }

public:
  // Construtor da classe que pede os pinos SDA e SCL como argumentos
  SensorVibracao(uint8_t pinSda, uint8_t pinScl)
    : _pinSda(pinSda), _pinScl(pinScl) {}

  bool estadoCompressor = false;  // Estado do compressor (ligado/desligado)

  // Inicializa o acelerômetro com uma faixa configurada
  void iniciar(uint8_t faixa = 2) {
    Wire.begin(_pinSda, _pinScl);
    Wire.beginTransmission(MMA8452_ADDRESS);
    Wire.write(REG_CTRL_REG1);
    Wire.write(0x01);
    Wire.endTransmission();

    uint8_t _faixa = 0x00;
    if (faixa == 4) _faixa = 0x01;
    else if (faixa == 8) _faixa = 0x02;

    Wire.beginTransmission(MMA8452_ADDRESS);
    Wire.write(REG_XYZ_DATA_CFG);
    Wire.write(_faixa);
    Wire.endTransmission();
  }

  // Verifica se o compressor está ligado (vibrando) ou desligado (não vibrando) com base em uma tolerância determinada
  bool verificarEstadoCompressor(float tolerancia = 0.5) {
    float x, y, z;
    _medirAceleracao(x, y, z);

    float _diferencaX = abs(x - _ultimoValorX);
    float _diferencaY = abs(y - _ultimoValorY);
    float _diferencaZ = abs(z - _ultimoValorZ);

    _ultimoValorX = (_ultimoValorX * 0.9) + (x * 0.1);
    _ultimoValorY = (_ultimoValorY * 0.9) + (y * 0.1);
    _ultimoValorZ = (_ultimoValorZ * 0.9) + (z * 0.1);

    if (_diferencaX > tolerancia || _diferencaY > tolerancia || _diferencaZ > tolerancia) {
      if (!estadoCompressor) {
        estadoCompressor = true;
        _tempoLigadoInicio = millis();
      }
      _ultimoRegistroVibracao = millis();
      return true;
      // Se não vibrar por mais de 500ms, considera o compressor desligado
    } else if (estadoCompressor && millis() - _ultimoRegistroVibracao > 500) {
      estadoCompressor = false;
      _tempoTotalLigado += millis() - _tempoLigadoInicio;
    }
    return false;
  }

  // Retorna o tempo total que o compressor esteve ligado
  unsigned long calcularTempoLigado() const {
    if (estadoCompressor) {
      return _tempoTotalLigado + (millis() - _tempoLigadoInicio);
    }
    return _tempoTotalLigado;
  }

  unsigned long retornaTempoTotalLigado() {
    return _tempoTotalLigado;
  }
};

// Classe para gerenciar o sensor de temperatura
class SensorTemperatura {
private:
  BME280I2C sensor;             // Instância para o sensor BME280
  float ultimaTemperatura = 0;  // Última temperatura registrada pelo sensor

public:
  // Inicia o sensor
  void iniciar() {
    sensor.begin();
    if (!sensor.begin()) {
      Serial.println("Erro ao iniciar sensor de temperatura!");  // Imprime um erro caso o sensor não inicie
    }
  }

  // Realiza a leitura da temperatura e a retorna
  float medirTemperatura() {
    // Mede a temperatura
    float temperatura = sensor.temp();
    if (isnan(temperatura)) {
      Serial.println("Erro ao ler temperatura!");
      return ultimaTemperatura;
    }
    ultimaTemperatura = temperatura;
    return temperatura;
  }
};

// Classe para gerenciar o display I2C LCD
class DisplayLCD {

public:
  // Inicializa o display
  void iniciar() {
    displayLCD.init();
    displayLCD.backlight();
    displayLCD.print("Iniciando...");
  }

  // Exibe a temperatura lida pelo sensor
  void exibirTemperatura(float temperatura) {
    displayLCD.clear();
    displayLCD.setCursor(0, 0);
    displayLCD.print("Temp compressor: ");
    displayLCD.setCursor(4, 1);
    displayLCD.print(temperatura);
    displayLCD.print(" C");
  }

  // Exibe o tempo de uso do óleo
  void exibirTempoUsoOleo(unsigned long tempoUsoOleo) {
    displayLCD.clear();
    displayLCD.setCursor(0, 0);
    displayLCD.print("Tempo Uso Oleo: ");
    displayLCD.setCursor(4, 1);
    displayLCD.print(tempoUsoOleo / 1000);
    displayLCD.print(" s");
  }

  // Exibe um erro qualquer
  void exibirErro() {
    displayLCD.clear();
    displayLCD.setCursor(0, 1);
    displayLCD.print("Erro no sensor!");
  }
};


// Classe principal que gerencia a dispositivo de monitoramento
class DispositivoCompressor {
private:
  // Instância das classes construídas anteriormente
  SensorVibracao sensorVibracao;
  SensorTemperatura sensorTemperatura;
  ConectorUbidots ubidots;
  DisplayLCD display;

  // Controle de tempo das atividades do dispositivo
  unsigned long _tempoUltimoRelatorio = 0;            // Momento em que foi realizada a última troca de óleo
  unsigned long _tempoIntervaloRelatorios = 1000;     // Intervalo para envio de dados
  unsigned long _tempoUltimaTrocaOleo = 0;            // Tempo desde a última troca de óleo
  unsigned long _tempoLimiteUsoOleo = 10000;          // Intervalo recomendado para troca de óleo
  unsigned long _tempoUltimaTrocaDisplay = 0;         // Controle do tempo para alternar display
  unsigned long _intervaloAlternanciaDisplay = 2000;  // Intervalo de alternância (2 segundos)
  bool _exibirTemperatura = true;                     // Controle para alternância de exibição

  float _limiteTemperatura = 28.0;  // Limite de temperatura

  bool _trocaOleoNecessaria = false;  // Indica necessidade de troca de óleo

  uint8_t _pinLEDVerde, _pinLEDVermelho, _pinLEDAmarelo;  // Pinos dos LEDs utilizados
  uint8_t _pinBotaoTrocaOleo;                             // Pino do botão para indicar troca de óleo
  uint8_t _pinBuzzer;                                     // Pino do buzzer utilizado

  unsigned long _tempoUsoOleo = 0;
  unsigned long _ultimaTroca = 0;
  unsigned long _tempoUltimaEnvioUbidots = 0;

  bool _trocouUmaVez = false;



  // Checa costantemente por quando a troca de óleo é realizada
  void _checarTrocaOleo() {
    unsigned long _tempoAtual = millis();

    unsigned long tempoTotalLigado = sensorVibracao.calcularTempoLigado();
    // if (sensorVibracao.verificarEstadoCompressor(0.1)) {
    //   _tempoUsoOleo = tempoTotalLigado + (_tempoAtual - millis()) - _ultimaTroca;
    // } else {
    //   _tempoUsoOleo = tempoTotalLigado - _ultimaTroca;
    // }

    if (_trocouUmaVez) {
      _tempoUsoOleo = tempoTotalLigado - _ultimaTroca;
    } else {
      _tempoUsoOleo = tempoTotalLigado;
    }

    if (_tempoUsoOleo > _tempoLimiteUsoOleo) {
      _trocaOleoNecessaria = true;
      digitalWrite(_pinLEDAmarelo, HIGH);
      Serial.println("Troca de óleo necessária!");
    }

    if (digitalRead(_pinBotaoTrocaOleo) == HIGH || botaoUbidots == true) {
      _ultimaTroca = tempoTotalLigado;
      botaoUbidots = false;
      _trocouUmaVez = true;
      _trocaOleoNecessaria = false;
      digitalWrite(_pinLEDAmarelo, LOW);
      Serial.println("Troca de óleo realizada.");
    }
  }

  // Checa constantemente se há superaquecimento do compressor
  void _checarSuperAquecimento(float temperatura) {
    if (temperatura > _limiteTemperatura) {
      digitalWrite(_pinLEDVermelho, HIGH);
      digitalWrite(_pinBuzzer, HIGH);
    } else {
      digitalWrite(_pinLEDVermelho, LOW);
      digitalWrite(_pinBuzzer, LOW);
    }
  }

public:
  // Construtor da classe que  leva os pinos SDA e SCL, o pino do botão para troca de óleo, os pinos para os LEDs verde, vermelho e amarelo, o pino para o buzzer como argumentos
  DispositivoCompressor(uint8_t pinSda, uint8_t pinScl, uint8_t pinBotaoTrocaOleo,
                        uint8_t pinLEDVerde, uint8_t pinLEDVermelho, uint8_t pinLEDAmarelo, uint8_t pinBuzzer)
    : sensorVibracao(pinSda, pinScl), _pinBotaoTrocaOleo(pinBotaoTrocaOleo),
      _pinLEDVerde(pinLEDVerde), _pinLEDVermelho(pinLEDVermelho), _pinLEDAmarelo(pinLEDAmarelo), _pinBuzzer(pinBuzzer) {}

  // Inicializa o dispositivo e todos os seus componentes
  void iniciar() {

    // Inicializa propriamente todos os LEDs
    pinMode(_pinLEDVerde, OUTPUT);
    pinMode(_pinLEDVermelho, OUTPUT);
    pinMode(_pinLEDAmarelo, OUTPUT);
    // Inicializa propriamente o botão para troca de óleo
    pinMode(_pinBotaoTrocaOleo, INPUT);
    // Inicializa propriamente o buzzer
    pinMode(_pinBuzzer, OUTPUT);

    // Inicializa o monitor serial
    Serial.begin(115200);

    // Inicializa os sensores
    sensorVibracao.iniciar();
    sensorTemperatura.iniciar();
    // Inicializa o display LCD
    display.iniciar();
    // Inicializa o ubidots
    ubidots.iniciar();
  }

  // Mantém as operações do dispositivo de forma contínua

  void operar() {
    // Mantém o Ubidots conectado
    ubidots.conectar();
    // Atualiza o controle de tempo
    unsigned long _tempoAtual = millis();

    // Alterna a exibição no display a cada 2 segundos
    if (_tempoAtual - _tempoUltimaTrocaDisplay >= _intervaloAlternanciaDisplay) {
      _tempoUltimaTrocaDisplay = _tempoAtual;
      _exibirTemperatura = !_exibirTemperatura;  // Alterna entre true e false
    }

    // Mantém o envio dos relatórios para o Ubidots com um intervalo de 500ms entre eles
    if (_tempoAtual - _tempoUltimoRelatorio >= _tempoIntervaloRelatorios) {
      _tempoUltimoRelatorio = _tempoAtual;

      bool _estadoCompressor = sensorVibracao.verificarEstadoCompressor(0.1);  // Verifica o estado do compressor continuamente
      float _temperatura = sensorTemperatura.medirTemperatura();               // Realiza a leitura da temperatura continuamente
      unsigned long _tempoLigado = sensorVibracao.calcularTempoLigado();       // Mantém atualizado o cálculo do tempo ligado

      if (!isnan(_temperatura)) {
        // Alterna entre exibir temperatura e tempo de uso do óleo
        if (_exibirTemperatura) {
          display.exibirTemperatura(_temperatura);
        } else {
          display.exibirTempoUsoOleo(_tempoUsoOleo);
        }
      } else {
        display.exibirErro();  // Exibe mensagem de erro caso não seja possível realizar alguma leitura
      }

      if (_estadoCompressor) {
        digitalWrite(_pinLEDVerde, HIGH);
      } else {
        digitalWrite(_pinLEDVerde, LOW);
      }


      // Faz a publicação de todas as variáveis observadas ou calculadas
      if (_tempoAtual - _tempoUltimaEnvioUbidots >= 2000) {
        _tempoUltimaEnvioUbidots = _tempoAtual;
        ubidots.publicar(ALERTA_TROCA_OLEO, _trocaOleoNecessaria);
        ubidots.publicar(TEMPERATURA, _temperatura);
      } else {
        ubidots.publicar(TEMPO_LIGADO, _tempoLigado / 1000);
        ubidots.publicar(TEMPO_USO_OLEO, _tempoUsoOleo / 1000);
        ubidots.publicar(ESTADO_COMPRESSOR, sensorVibracao.estadoCompressor);
      }

      // Checa constantemente pela troca de óleo
      _checarTrocaOleo();
      
      // Checa constantemente pelo superaquecimento da máquina
      _checarSuperAquecimento(_temperatura);
    }
  }
};


// Instância do compressor
DispositivoCompressor dispositivo(21, 22, 33, 16, 18, 17, 32);




// Inicia o dispositivo
void setup() {
  dispositivo.iniciar();
}

// Mantém o dispositivo funcionando em loop
void loop() {
  dispositivo.operar();
}