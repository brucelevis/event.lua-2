/*
\brief �ļ�������Ҫ�޸�
\file WordFilterUtil.h

Date:2017��5��23��
Author:������

Copyright (c) 2017 ������Ϸ. All rights reserved.
*/
#ifndef _WORDFILTERUTIL_H
#define _WORDFILTERUTIL_H

#include <string>
#include <vector>
#include <set>
#include <map>
#include "String.h"

//�����ֹ�����
class WordFilterUtil
{
public:
	WordFilterUtil();
	~WordFilterUtil();
	//����������
	std::string filter(const std::string& input, int& replaceCount);
	//���һ���ؼ���
	void addKeywords(const std::string& keywords);
public:
	//��һ��Ϊ������Ϣ���ڶ���Ϊ��ʼ�ַ�
	typedef std::pair<unsigned char, utf32> LengthInfo;
	typedef std::vector<LengthInfo> LengthInfoList;
	//��һ��Ϊ��β�ַ����ڶ���Ϊ������Ϣ��������Ϣ��Ҫ��֤�ǽ�������
	typedef std::map<utf32, LengthInfoList> WordMap;
	//�������б���һ��Ϊ�����֣��ڶ���������
	typedef std::set<String, String::FastLessCompare> KeywordsMap;

protected:
	WordMap				mWordMap;
	KeywordsMap			mKeywordsMap;
};


#endif //_WORDFILTERUTIL_H