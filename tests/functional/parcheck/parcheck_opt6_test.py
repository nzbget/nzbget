nzbget_options = ['ParCheck=force', 'ParQuick=no', 'PostStrategy=balanced']

def test_parchecker_healthy(nserv, nzbget):
	hist = nzbget.download_nzb('parchecker.nzb')
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parchecker_repair(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker.nzb')
	nzb_content = nzb_content.replace('parchecker/testfile.dat?1=0:3000', 'parchecker/testfile.dat?1=0:3000!0')
	hist = nzbget.download_nzb('parchecker.repair.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/PAR'
