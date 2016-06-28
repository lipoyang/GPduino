/*
This software is available under NYSL(Nirunari Yakunari Sukinishiro License).
*/

#include <Servo.h>
#include <EEPROM.h>
#include "common.h"
#include "MovingAverage.h"

// ピン番号 (デジタル)
#define MOTOR_L_IN1    2
#define SERVO0_PWM     3
#define MOTOR_L_IN2    4
#define MOTOR_L_PWM    5
#define MOTOR_R_PWM    6
#define MOTOR_R_IN1    7
#define MOTOR_R_IN2    8
#define SERVO1_PWM     9
#define SERVO2_PWM     10

// ピン番号 (アナログ)
#define ANALOG_0      0
#define ANALOG_1      1
#define ANALOG_2      2
#define ANALOG_3      3
#define MODE_CHECK    6
#define BATT_CHECK    7

// サーボ
Servo servo[3];

// バッテリー電圧チェック
MovingAverage Vbat_MovingAve;

// モード
static int drive_mode;
#define MODE_TANK   0   // 戦車モード
#define MODE_CAR    1   // 自動車モード

// 戦車モードのプロポ状態保持用
static int g_fb;  // 前後方向
static int g_lr;  // 左右方向

// 暴走チェックカウンタ
static int cnt_runaway;

// 3.3V未満でローバッテリーとする
// (3.3V / 2) / 3.3V * 1024 =  512
#define LOW_BATTERY    512

// 四輪操舵のモード
static int g_steer_pol_f;
static int g_steer_pol_b;

// サーボの調整
static int g_servo_pol[3]; // 極性
static int g_servo_ofs[3]; // オフセット
static int g_servo_amp[3]; // 振幅

// 送信バッファ
static char txbuff[256];

/**
 * バッテリー電圧チェック
 */
void battery_check()
{
    static int cnt = 0;
    
    unsigned short Vbat = analogRead(BATT_CHECK);
    unsigned short Vbat_ave = Vbat_MovingAve.pop(Vbat);
    
    // 1秒ごとに電圧値送信
    cnt++;
    if(cnt >= 1000)
    {
        cnt=0;
        
        txbuff[0]='#';
        txbuff[1]='B';
        Uint16ToHex(&txbuff[2], Vbat_ave, 3);
        txbuff[5]='$';
        txbuff[6]='\0';
        Serial.print(txbuff);
    }
    
    // 電圧が低下したら停止
    if(Vbat_ave < LOW_BATTERY){
        // モータ停止
        analogWrite(MOTOR_L_PWM, 0);
        analogWrite(MOTOR_R_PWM, 0);
        digitalWrite(MOTOR_L_IN1, LOW);
        digitalWrite(MOTOR_L_IN2, LOW);
        digitalWrite(MOTOR_R_IN1, LOW);
        digitalWrite(MOTOR_R_IN2, LOW);
        // 復帰しない
        while(true){
            ;
        }
    }
}

/**
 * 暴走チェック
 */
void runaway_check()
{
    cnt_runaway++;
    
    // 1秒間コマンドが来なければモータ停止
    if(cnt_runaway > 1000)
    {
      analogWrite(MOTOR_L_PWM, 0);
      analogWrite(MOTOR_R_PWM, 0);
      digitalWrite(MOTOR_L_IN1, LOW);
      digitalWrite(MOTOR_L_IN2, LOW);
      digitalWrite(MOTOR_R_IN1, LOW);
      digitalWrite(MOTOR_R_IN2, LOW);
    }
}

/*
 * サーボの初期化
 */
void servo_init()
{
    int i;
    if(EEPROM.read(0) == 0xA5){
        for(i=0;i<3;i++){
            g_servo_pol[i] = (int)((signed char)EEPROM.read(i*3 + 1));
            g_servo_ofs[i] = (int)((signed char)EEPROM.read(i*3 + 2));
            g_servo_amp[i] =                    EEPROM.read(i*3 + 3);
        }
    }else{
        EEPROM.write(0, 0xA5);
        for(i=0;i<3;i++){
            g_servo_pol[i] = 1;
            g_servo_ofs[i] = 0;
            g_servo_amp[i] = 90;
            EEPROM.write(i*3 + 1, g_servo_pol[i]);
            EEPROM.write(i*3 + 2, g_servo_ofs[i]);
            EEPROM.write(i*3 + 3, g_servo_amp[i]);
        }
    }
    servo[0].attach(SERVO0_PWM);
    servo[1].attach(SERVO1_PWM);
    servo[2].attach(SERVO2_PWM);
    servo_ctrl(0,0);
    servo_ctrl(1,0);
    servo_ctrl(2,0);
}

/*
 * サーボ制御
 */
void servo_ctrl(int ch, int val)
{
    int deg = 90 + ((val + g_servo_ofs[ch]) *  g_servo_amp[ch]) / 127 * g_servo_pol[ch];
    
    // sprintf(txbuff, "%4d %4d / %4d %4d %4d", val, deg, g_servo_ofs[ch], g_servo_amp[ch], g_servo_pol[ch]);
    // Serial.println(txbuff);
    
    servo[ch].write(deg);
}

// 初期設定
void setup() {
    // シリアル通信の設定
    SerialCom_init();

    // PWMの初期化
    analogWrite(MOTOR_L_PWM, 0);
    analogWrite(MOTOR_R_PWM, 0);
    
    // GPIOの初期化
    digitalWrite(MOTOR_L_IN1, LOW);
    digitalWrite(MOTOR_L_IN2, LOW);
    digitalWrite(MOTOR_R_IN1, LOW);
    digitalWrite(MOTOR_R_IN2, LOW);
    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    
    // モード判定
    int mode_check = analogRead( MODE_CHECK );
    drive_mode = (mode_check > 512) ? MODE_CAR : MODE_TANK;

    if(drive_mode == MODE_CAR){
        // サーボの初期化
        servo_init();
    }
    
    // 変数初期化
    g_fb = 0;
    g_lr = 0;
    cnt_runaway = 0;
    Vbat_MovingAve.init();
    g_steer_pol_f = 1;
    g_steer_pol_b = 0;
}

// メインループ
void loop() {
    
    // シリアル受信
    SerialCom_loop();
    // バッテリー電圧チェック
    battery_check();
    // 暴走チェック
    runaway_check();
    
    delay(1);
}

/*
 * モータの制御
 */
void ctrl_motor(int ch, int val)
{
    static const int IN1[]={MOTOR_L_IN1, MOTOR_R_IN1};
    static const int IN2[]={MOTOR_L_IN2, MOTOR_R_IN2};
    static const int PWM[]={MOTOR_L_PWM, MOTOR_R_PWM};
    
    int l,r;
    int lpwm,rpwm,lin1,lin2,rin1,rin2;
    
    int pwm = (abs(val) << 1) + 1;
    if(pwm<0) pwm = 0;
    if(pwm>255) pwm = 255;
    
    int in1,in2;
    if(val > 0){
        in1 = LOW;
        in2 = HIGH;
    }else if(val < 0){
        in1 = HIGH;
        in2 = LOW;
    }else{
        in1 = LOW;
        in2 = LOW;
        pwm = 0;
    }

    digitalWrite(IN1[ch], in1);
    digitalWrite(IN2[ch], in2);
    analogWrite(PWM[ch], pwm);
}

/**
 * 戦車のモータ制御
 */
void ctrl_tank()
{
    int l,r;
    int lpwm,rpwm,lin1,lin2,rin1,rin2;
    
    r = (int)(g_fb - g_lr/2);
    l = (int)(g_fb + g_lr/2);
    
    // sprintf(txbuff, "%4d %4d %4d %4d", l, r, g_fb, g_lr);
    // Serial.println(txbuff);
    
    ctrl_motor(0, l);
    ctrl_motor(1, r);
}

/**
 * 受信したコマンドの実行
 *
 * @param buff 受信したコマンドへのポインタ
 */
void SerialCom_callback(char* buff)
{
    unsigned short val;
    int sval, sval2;
    int deg;
    int ch;
    int i;
    
    cnt_runaway = 0; // 暴走チェックカウンタのクリア
    
    // Serial.println(buff); // TEST
    
    switch(buff[0])
    {
    /* Dコマンド(前進/後退)
       書式: #Dxx$
       xx: 0のとき停止、正のとき前進、負のとき後退。
     */
    case 'D':
        // 値の解釈
        if( HexToUint16(&buff[1], &val, 2) != 0 ) break;
        sval = (int)((signed char)val);
        
        // 自動車モードの場合
        if(drive_mode == MODE_CAR)
        {
            ctrl_motor(0, sval);
            ctrl_motor(1, sval);
        }
        // 戦車モードの場合
        else
        {
            g_fb = sval;
            ctrl_tank();
        }
        break;
        
    /* Tコマンド(旋回)
       書式: #Txxn$
       n: 4WSモード。0のとき後輪固定、1のとき同相、2のとき逆相
       xx: 0のとき中立、正のとき右旋回、負のとき左旋回
     */
    case 'T':
        // 4WSモード
        switch(buff[3]){
        case '0':// FRONT
            g_steer_pol_f = 1;
            g_steer_pol_b = 0;
            break;
        case '1':// COMMON
            g_steer_pol_f = 1;
            g_steer_pol_b = 1;
            break;
        case '2':// REVERSE
            g_steer_pol_f = 1;
            g_steer_pol_b = -1;
            break;
        case '3':// REAR
            g_steer_pol_f = 0;
            g_steer_pol_b = 1;
            break;
        default:
            g_steer_pol_f = 1;
            g_steer_pol_b = 0;
            break;
        }
        // 値の解釈
        if( HexToUint16(&buff[1], &val, 2) != 0 ) break;
        sval = (int)((signed char)val);
        
        // 自動車モードの場合
        if(drive_mode == MODE_CAR)
        {
            servo_ctrl(0, sval);
            servo_ctrl(1, g_steer_pol_f * sval);
            servo_ctrl(2, g_steer_pol_b * sval);
        }
        // 戦車モードの場合
        else
        {
            g_lr = sval;
            ctrl_tank();
        }
        break;
        
    /* Mコマンド(モータ制御)
       書式1: #Mnxx$
       n: '1'はモータ1(左)、'2'はモータ2(右)
       xx: 0のとき停止、正のとき正転、負のとき反転
       
       書式2: #MAxxyy$
       xx: モータ1(左)  0のとき停止、正のとき正転、負のとき反転
       yy: モータ2(右)  0のとき中立、正のとき正転、負のとき反転
     */
    case 'M':
        switch(buff[1]){
        case '1':
            // 値の解釈
            if( HexToUint16(&buff[2], &val, 2) != 0 ) break;
            sval = (int)((signed char)val);
            ctrl_motor(0, sval);
            break;
        case '2':
            // 値の解釈
            if( HexToUint16(&buff[2], &val, 2) != 0 ) break;
            sval = (int)((signed char)val);
            ctrl_motor(1, sval);
            break;
        case 'A':
            // 値の解釈
            if( HexToUint16(&buff[2], &val, 2) != 0 ) break;
            sval = (int)((signed char)val);
            if( HexToUint16(&buff[4], &val, 2) != 0 ) break;
            sval2 = (int)((signed char)val);
            ctrl_motor(0, sval);
            ctrl_motor(1, sval2);
            break;
        }
        break;
        
    /* Sコマンド(サーボ制御)
       書式: #Sxxn$
       xx: 0のとき中立、正のとき正転、負のとき反転
       n: サーボチャンネル 0～2
     */
    case 'S':
        // 値の解釈
        if( HexToUint16(&buff[1], &val, 2) != 0 ) break;
        sval = (int)((signed char)val);
        // チャンネル
        ch = (buff[3] == '1') ? 1 : ((buff[3] == '2') ? 2 : 0);
        // サーボの制御
        servo_ctrl(ch, sval);
        break;
        
    /* Aコマンド(サーボの調整)
        (1)サーボ極性設定
           書式: #APxn$
           n: サーボチャンネル 0～2
           xは'+'または'-'。'+'は正転、'-'は反転。初期値は正転。
        (2)サーボオフセット調整
           書式: #AOxxn$
           n: サーボチャンネル 0～2
           xx:中央位置のオフセット
        (3)サーボ振幅調整
           書式: #AAnxx$
           n: サーボチャンネル 0～2
           xx:振れ幅
        (4)サーボ設定値保存
           書式: #AS$
           サーボの設定値をEEPROMに保存する
        (5)サーボ設定値読み出し
            書式: #AL$
           サーボの設定値をEEPROMから読み出し、送信する
     */
    case 'A':
        switch(buff[1]){
        case 'P':
            ch = (buff[3] == '1') ? 1 : ((buff[3] == '2') ? 2 : 0);
            if(buff[2] == '+'){
                g_servo_pol[ch] = 1;
            }else if(buff[2] == '-'){
                g_servo_pol[ch] = -1;
            }
            break;
        case 'O':
            ch = (buff[4] == '1') ? 1 : ((buff[4] == '2') ? 2 : 0);
            // 値の解釈
            if( HexToUint16(&buff[2], &val, 2) != 0 ) break;
            g_servo_ofs[ch] = (int)((signed char)val);
            break;
        case 'A':
            ch = (buff[4] == '1') ? 1 : ((buff[4] == '2') ? 2 : 0);
            // 値の解釈
            if( HexToUint16(&buff[2], &val, 2) != 0 ) break;
            g_servo_amp[ch] = (int)((unsigned char)val);
            break;
        case 'S':
            for(i=0;i<3;i++){
                EEPROM.write(i*3 + 1, g_servo_pol[i]);
                EEPROM.write(i*3 + 2, g_servo_ofs[i]);
                EEPROM.write(i*3 + 3, g_servo_amp[i]);
            }
            break;
        case 'L':
            //delay(200); // バッファ詰まりの応急処置 (よろしくない)
            txbuff[0]='#';
            txbuff[1]='A';
            txbuff[2]='L';
            for(i=0;i<3;i++){
                g_servo_pol[i] = (int)((signed char)EEPROM.read(i*3 + 1));
                g_servo_ofs[i] = (int)((signed char)EEPROM.read(i*3 + 2));
                g_servo_amp[i] =                    EEPROM.read(i*3 + 3);
                txbuff[3+i*5] = (g_servo_pol[i]==1) ? '+' : '-';
                Uint16ToHex(&txbuff[4+i*5], (unsigned short)((unsigned char)g_servo_ofs[i]), 2);
                Uint16ToHex(&txbuff[6+i*5], (unsigned short)(               g_servo_amp[i]), 2);
            }
            txbuff[18]='$';
            txbuff[19]='\0';
            Serial.print(txbuff);
            //delay(200); // バッファ詰まりの応急処置 (よろしくない)
            break;
        }
        break;
    }
}
