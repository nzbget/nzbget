nzbget_options = ['SaveQueue=no']

import os

nzbget = None

def prepare_queue(nzbget_instance):
	global nzbget

	if nzbget != None:
		nzbget.api.editqueue('GroupSort', 'Name', [])
		check_list([1,2,3,4,5,6,7,8,9])
		return

	print('prepare_queue')
	nzbget = nzbget_instance

	nzbget.api.pausedownload()

	nzbget_srcdir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
	fullfilename = nzbget_srcdir + '/tests/testdata/nzbfile/plain.nzb'
	in_file = open(fullfilename, 'r')
	nzb_content = in_file.read()
	in_file.close()

	nzbget.append_nzb('1', nzb_content)
	nzbget.append_nzb('2', nzb_content)
	nzbget.append_nzb('3', nzb_content)
	nzbget.append_nzb('4', nzb_content)
	nzbget.append_nzb('5', nzb_content)
	nzbget.append_nzb('6', nzb_content)
	nzbget.append_nzb('7', nzb_content)
	nzbget.append_nzb('8', nzb_content)
	nzbget.append_nzb('9', nzb_content)

def check_list(req):
	queue = nzbget.api.listgroups()
	assert len(req) == len(queue)
	for i in range(len(req)):
		assert req[i] == queue[i]['NZBID']

def test_editqueue_move_offset(nzbget):
	prepare_queue(nzbget)

	check_list([1,2,3,4,5,6,7,8,9])

	nzbget.api.editqueue('GroupMoveOffset', '1', [2])
	check_list([1,3,2,4,5,6,7,8,9])

	nzbget.api.editqueue('GroupMoveOffset', '-1', [2])
	check_list([1,2,3,4,5,6,7,8,9])

	nzbget.api.editqueue('GroupMoveOffset', '2', [4,5,6])
	check_list([1,2,3,7,8,4,5,6,9])

	nzbget.api.editqueue('GroupMoveOffset', '-2', [4,5,6])
	check_list([1,2,3,4,5,6,7,8,9])

	nzbget.api.editqueue('GroupMoveOffset', '5', [4,6])
	check_list([1,2,3,5,7,8,9,4,6])

	nzbget.api.editqueue('GroupMoveOffset', '-5', [2,5,7])
	check_list([2,5,7,1,3,8,9,4,6])

def test_editqueue_move_before_after(nzbget):
	prepare_queue(nzbget)

	check_list([1,2,3,4,5,6,7,8,9])

	nzbget.api.editqueue('GroupMoveBefore', '2', [8,9])
	check_list([1,8,9,2,3,4,5,6,7])

	nzbget.api.editqueue('GroupMoveBefore', '2', [8,9])
	check_list([1,8,9,2,3,4,5,6,7])

	nzbget.api.editqueue('GroupMoveAfter', '2', [8,9])
	check_list([1,2,8,9,3,4,5,6,7])

	nzbget.api.editqueue('GroupMoveAfter', '7', [1,8,9])
	check_list([2,3,4,5,6,7,1,8,9])

	nzbget.api.editqueue('GroupMoveBefore', '2', [5,1,9])
	check_list([5,1,9,2,3,4,6,7,8])

	nzbget.api.editqueue('GroupMoveBefore', '2', [5,1,9])
	check_list([5,1,9,2,3,4,6,7,8])

	nzbget.api.editqueue('GroupMoveAfter', '2', [5,1,9])
	check_list([2,5,1,9,3,4,6,7,8])

	nzbget.api.editqueue('GroupMoveAfter', '8', [5,2])
	check_list([1,9,3,4,6,7,8,2,5])
