#include "WordFilterUtil.h"
#include <algorithm>

WordFilterUtil::WordFilterUtil()
{

}

WordFilterUtil::~WordFilterUtil()
{
}

//����������
std::string WordFilterUtil::filter(const std::string& input, int& replaceCount)
{
	replaceCount = 0;
	String temp;
	int tIndex = 0;//1�γɹ����˵�����
				   //std::string u = _g2u(input);
	String utf8Str = (utf8*)input.c_str();
	for (int index = 0; index < utf8Str.size(); index++)
	{
		const utf32& code = utf8Str[index];
		WordMap::iterator wi = mWordMap.find(code);
		if (wi != mWordMap.end())
		{
			LengthInfoList& list = wi->second;
			LengthInfoList::iterator i, iend = list.end();
			for (i = list.begin(); i != iend; ++i)
			{
				LengthInfo& info = *i;
				int fStartIndex = index - (info.first - 1);
				if (info.first <= (index + 1) && utf8Str[fStartIndex] == info.second)
				{
					String keyword;
					for (int p = 0; p < info.first; p++)
					{
						keyword.push_back(utf8Str[fStartIndex + p]);

					}
					if (keyword.size() != 0)
					{
						KeywordsMap::iterator it = mKeywordsMap.find(keyword);
						if (it != mKeywordsMap.end())
						{
							//�ɹ�����
							for (int k = tIndex; k <= index; k++)
							{
								const utf32& c = utf8Str[k];
								if (k >= fStartIndex)
									temp.append("*");
								else temp.push_back(c);
							}
							++replaceCount;

							tIndex = index + 1;

							//�����������б���ֻ����һ��
							break;
						}
					}
				}
			}
		}
	}
	if (tIndex == 0)
	{
		//û���κι���
		replaceCount = 0;
		return input;
	}
	//�������δ�ɹ����˵��ַ�
	if (tIndex < utf8Str.size())
	{
		for (int p = tIndex; p < utf8Str.size(); p++)
		{
			const utf32& code = utf8Str[p];
			temp.push_back(code);
		}
	}
	std::string v = temp.c_str();
	//v = _u2g(v);
	return v;
}

struct greater
{
	bool operator()(const WordFilterUtil::LengthInfo& _Left, const WordFilterUtil::LengthInfo& _Right) const
	{
		return (_Left.first > _Right.first);
	}
};
//���һ���ؼ���
void WordFilterUtil::addKeywords(const std::string& keywords)
{
	//�����ַ���Ϣ
	//std::string u = _g2u(keywords);
	String utf8Str = (utf8*)keywords.c_str();
	const utf32& lastCode = utf8Str[utf8Str.size() - 1];
	WordMap::iterator wi = mWordMap.find(lastCode);
	if (wi == mWordMap.end())
	{
		mWordMap.insert(WordMap::value_type(lastCode, LengthInfoList()));
		wi = mWordMap.find(lastCode);
	}

	LengthInfoList& list = wi->second;
	LengthInfo info;
	info.first = utf8Str.size();
	info.second = utf8Str[0];
	list.push_back(info);
	//��������
	std::sort(list.begin(), list.end(), greater());
	//����ؼ���
	if (mKeywordsMap.find(utf8Str) == mKeywordsMap.end())
	{
		mKeywordsMap.insert(KeywordsMap::value_type(utf8Str));
	}
}
