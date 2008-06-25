#ifndef ROSTERLABELORDERS_H
#define ROSTERLABELORDERS_H

//�������������� ����������� �������
#define RLID_FOOTER_TEXT                          -5
#define RLID_DISPLAY                              -4
#define RLID_DECORATION                           -3
#define RLID_INDICATORBRANCH                      -2
#define RLID_NULL                                 -1

//����� ������������� ����� �� ������
#define RLAP_LEFT_CENTER                          00000
//����� ������������� �����
#define RLAP_LEFT_TOP                             10000
//����� ������������� ������
#define RLAP_RIGHT_TOP                            20000
//����� ������������� ������ �� ������
#define RLAP_RIGHT_CENTER                         30000

/*************************************************************************/
#define RLO_AVATAR_IMAGE_LEFT                     RLAP_LEFT_CENTER + 100
#define RLO_DECORATION                            RLAP_LEFT_CENTER + 500

#define RLO_CONNECTING                            RLAP_LEFT_TOP + 100
#define RLO_MESSAGE                               RLAP_LEFT_TOP + 200
#define RLO_SUBSCRIBTION                          RLAP_LEFT_TOP + 300
#define RLO_DISPLAY                               RLAP_LEFT_TOP + 500

#define RLO_PRIVACY                               RLAP_RIGHT_TOP + 300
#define RLO_CONNECTION_ENCRYPTED                  RLAP_RIGHT_TOP + 500

#define RLO_AVATAR_IMAGE_RIGHT                    RLAP_RIGHT_CENTER + 900

#endif