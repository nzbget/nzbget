nzbget_options = ['ArticleCache=500', 'DirectWrite=no']

def test_1kb(nserv, nzbget):
	hist = nzbget.download_nzb('1k.dat.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_1gb_unpack(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1gb.nzb')
	hist = nzbget.download_nzb('1gb_unpack.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

