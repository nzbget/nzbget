nzbget_options = ['ParCheck=auto', 'ParQuick=yes', 'PostStrategy=sequential']

def test_parchecker_healthy(nserv, nzbget):
	hist = nzbget.download_nzb('parchecker.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_parchecker_repair(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker.nzb')
	nzb_content = nzb_content.replace('parchecker/testfile.dat?1=0:3000', 'parchecker/testfile.dat?1=0:3000!0')
	hist = nzbget.download_nzb('parchecker.repair.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parchecker_subject(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker.nzb')
	nzb_content = nzb_content.replace('parchecker/testfile.dat?1=0:3000', 'parchecker/testfile.dat?1=0:3000!0')
	nzb_content = nzb_content.replace('subject="&quot;', 'subject="')
	nzb_content = nzb_content.replace('&quot; yEnc', '.dat yEnc')
	hist = nzbget.download_nzb('parchecker.subject.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/PAR'
