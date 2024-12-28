// importando a biblioteca do sensor de temperatura
#include "DHT.h"

DHT dht(21, DHT22);  // Inicializa o sensor DHT

// cria a classe sensor de temperatura
class SensorTemp 
{
  private:
    DHT& sensor;
    
    int medirTemp() 
    {
      delay(2000);
      float t = sensor.readTemperature();
      // condição para verificar se o sensor foi realmente ligado
      if (isnan(t)) 
      {
        Serial.println(F("Failed to read from DHT sensor!"));
      } 
      else 
      {
        Serial.print(F("Temperature: "));
        Serial.print(t);
        Serial.print(F("°C \n"));
      }

      return t;
    }

  public:
    // Construtor
    SensorTemp(DHT& sensorObject) : sensor(sensorObject){}
    // método para medir a temperatura e imprimir ela
    bool alertarCrise()
    {
      float t = medirTemp();

      if(t>=50)
      {
        return true;
      }
      else
      {
        return false;
      }
    }
};


//criando a classe botão
class Botao {
  private:
    int pin;
    bool estadoAtual;
    int ultimoEstado;
  public:
    // Construtor
    Botao(int numPin)
    {
      pin = numPin;
      pinMode(pin, INPUT);  
      estadoAtual = false;  
      ultimoEstado = LOW;   
    }
    int atualiza()
    {
      int leitura = digitalRead(pin);
      // verifica se o botão foi pressionado 
      if (leitura == HIGH && ultimoEstado == LOW)
      {
        alternaEstado(); //chama a função de alterar o estado do botão
        
      }
      ultimoEstado = leitura;  // atualiza o último estado do botão
      return ultimoEstado;  
    }

    void alternaEstado()
    {
      // altera o estado do botão
      estadoAtual = !estadoAtual;
      if (estadoAtual)
      {
        Serial.println(F("Ligando Máquina"));
      }
      else
      {
        Serial.println(F("Desligando Máquina"));
      }
    }
};

// função para medir distância
int medirDistancia() {
  //comandos para medir a distância a partir de um sonar 
  digitalWrite(18, LOW);
  delayMicroseconds(2);
  digitalWrite(18, HIGH);
  delayMicroseconds(10);
  digitalWrite(18, LOW);
  // coletando o tempo de duração que teve um pulso
  int duration = pulseIn(17, HIGH);
  //calcula a distância a partir da duração do pulso
  float distance = duration * 0.034 / 2;
  //imprimindo essa distância
  Serial.print("Distância: ");
  Serial.print(distance);
  Serial.print(" cm \n");
  return distance;
}



// instânciando as classes
Botao botaoOnOff(14); // classe do botão
SensorTemp sensorTemp(dht); // classe do sensor de temp


void setup() 
{
  Serial.begin(9600);
  // definindo as portas dos LED's
  pinMode(25, OUTPUT);
  pinMode(26, OUTPUT);
  pinMode(18, OUTPUT);
  //definindo a porta do botão
  pinMode(17, INPUT);

  //inicializando o sensor de temperatura
  dht.begin();
}

void loop() 
{
  // analisando se o botão está ligado ou não
  if(botaoOnOff.atualiza() == HIGH)
  {
    // puxando a função que mede a distância e guarda a distância em uma variável
    int distance = medirDistancia();
    
    bool criseTemp = sensorTemp.alertarCrise();
    if (criseTemp) 
    {
      digitalWrite(25, HIGH);
      digitalWrite(26, HIGH);
      delay(300);
      digitalWrite(25, LOW);
      digitalWrite(26, LOW);
      Serial.println("Máquina hiperaquecendo");
    }
    else 
    {
      digitalWrite(25, HIGH);
      digitalWrite(26, LOW);
    };

  } 
  else{
    //indica se o botão está desligado avisando no SerialMonitor
    digitalWrite(25, LOW);
    digitalWrite(26, HIGH);
    Serial.println("Máquina Desligada");
    delay(1500);
  };
}