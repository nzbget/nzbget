import base64

nzbget_options = ['HealthCheck=none', 'DupeCheck=yes']

def test_dupecheck_small_copy(nserv, nzbget):
	hist = nzbget.download_nzb('small.nzb', dupemode = 'SCORE')
	assert hist['Status'] == 'SUCCESS/HEALTH'
	nzb_content = nzbget.load_nzb('small.nzb')
	hist = nzbget.download_nzb('small.copy.nzb', nzb_content, dupemode = 'SCORE')
	assert hist['Status'] == 'DELETED/COPY'

def test_dupecheck_small_id(nserv, nzbget):
	nzbget.clear()
	hist = nzbget.download_nzb('small.nzb', dupemode = 'SCORE')
	assert hist['Status'] == 'SUCCESS/HEALTH'
	nzb_content = nzbget.load_nzb('small.nzb')
	nzbcontent64 = base64.standard_b64encode(nzb_content)
	id = nzbget.api.append('small.copy2.nzb', nzbcontent64, 'test', 0, False, False, '', 0, 'SCORE', [])
	assert id > 0
