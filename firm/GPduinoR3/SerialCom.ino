/*
This software is available under NYSL(Nirunari Yakunari Sukinishiro License).
*/

// シリアルコマンド電文の受信処理

// 0x02/0x03だとデバッグしにくいので、#/$ を 電文開始/終了 に使う
// 電文開始
#define CODE_STX '#'
// 電文終了
#define CODE_ETX '$'

// 受信状態
static int state;
// 電文開始待ち状態
#define STATE_READY     0
// 電文受信中状態
#define STATE_RECEIVING 1

// 受信バッファ
#define BUFF_SIZE 32
static int ptr=0;
static char buff[BUFF_SIZE];

// 受信コマンド処理
extern void SerialCom_callback(char* buff);

// 初期化
void SerialCom_init()
{
    state = STATE_READY;
    
    // シリアル通信の設定
//  Serial.begin(9600);
//  Serial.begin(19200);
    Serial.begin(38400);
}


// 受信ループ処理
void SerialCom_loop() {

    char c;
    
    /* シリアル受信データがあるか？ */
    while (Serial.available() > 0)
    {
        //Serial.println("RECV ");
        c = Serial.read();
        switch(state)
        {
        /* 電文開始待ち状態 */
        case STATE_READY:
            /* 電文開始コードが来たら電文受信中状態へ */
            if(c == CODE_STX)
            {
                //Serial.println("STX ");
                state = STATE_RECEIVING;
                ptr = 0;
            }
            break;
        /* 電文受信中状態 */
        case STATE_RECEIVING:
            /* もしも電文開始コードが来たら受信中のデータを破棄 */
            if(c == CODE_STX)
            {
                //Serial.println("STX ");
                ptr = 0;
            }
            /* 電文終了コードが来たら、受信した電文のコマンドを実行 */
            else if(c == CODE_ETX)
            {
                //Serial.println("ETX ");
                buff[ptr] = '\0';
                SerialCom_callback(buff);
                state = STATE_READY;
            }
            /* 1文字受信 */
            else
            {
                buff[ptr] = c;
                ptr++;
                if(ptr>=BUFF_SIZE)
                {
                    state = STATE_READY;
                }
            }
            break;
        default:
            state = STATE_READY;
        }
    }
}

