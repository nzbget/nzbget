nzbget_options = ['ArticleCache=500', 'DirectWrite=no']

def test_small(nserv, nzbget):
	hist = nzbget.download_nzb('small.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_medium_unpack(nserv, nzbget):
	nzb_content = nzbget.load_nzb('medium.nzb')
	hist = nzbget.download_nzb('medium_unpack.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

